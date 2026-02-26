/* eslint-disable no-undef */
class XorShift32 {
  constructor(seed) {
    this.state = (seed >>> 0) || 0x12345678;
  }
  nextU32() {
    let x = this.state >>> 0;
    x ^= x << 13;
    x ^= x >>> 17;
    x ^= x << 5;
    this.state = x >>> 0;
    return this.state;
  }
  nextFloat() {
    return (this.nextU32() >>> 0) / 0xffffffff;
  }
  nextSigned() {
    return this.nextFloat() * 2 - 1;
  }
}

function clamp(x, a, b) {
  return Math.min(b, Math.max(a, x));
}
function softClip(x) {
  return Math.tanh(x);
}

class CdProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: "mode", defaultValue: 0, minValue: 0, maxValue: 3, automationRate: "k-rate" }, // 0 hold,1 mute,2 interp,3 repeat
      { name: "errorRate", defaultValue: 0.18, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "burstMs", defaultValue: 24, minValue: 1, maxValue: 400, automationRate: "k-rate" },
      { name: "repeatMs", defaultValue: 42, minValue: 1, maxValue: 400, automationRate: "k-rate" },
      { name: "scratchRate", defaultValue: 0.25, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "scratchAmt", defaultValue: 0.35, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "jitterMs", defaultValue: 0.18, minValue: 0, maxValue: 2, automationRate: "k-rate" },
      { name: "jitterRate", defaultValue: 38, minValue: 1, maxValue: 200, automationRate: "k-rate" },
      { name: "hfLoss", defaultValue: 0.1, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "servoNoise", defaultValue: 0.12, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "softClip", defaultValue: 1, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "ceiling", defaultValue: 0.94, minValue: 0.2, maxValue: 1, automationRate: "k-rate" },
      { name: "outGain", defaultValue: 0.98, minValue: 0, maxValue: 1.5, automationRate: "k-rate" },
    ];
  }

  constructor(options) {
    super();
    const seed = (options?.processorOptions?.seed ?? 0x4344454e) >>> 0;
    this.prng = new XorShift32(seed);

    // Small delay line for jitter (fractional delay).
    this.delayLen = Math.max(256, Math.ceil(sampleRate * 0.01));
    this.delay = new Float32Array(this.delayLen);
    this.di = 0;
    this.jPhase = this.prng.nextFloat();
    this.jNoise = 0;

    // Concealment state.
    this.errRemain = 0;
    this.errTotal = 0;
    this.lastGood = 0;
    this.errStart = 0;
    this.errEnd = 0;
    this.errEndSample = 0;

    // Ring buffer for repeat frames (up to 300ms).
    this.ringLen = Math.max(1024, Math.ceil(sampleRate * 0.35));
    this.ring = new Float32Array(this.ringLen);
    this.ri = 0;

    // Scratch transient state.
    this.clickRemain = 0;
    this.clickTotal = 0;
    this.clickAmp = 0;
    this.clickSign = 1;

    // Noise shaping.
    this.servoPhase = this.prng.nextFloat();
    this.servoPhase2 = this.prng.nextFloat();
    this.hfZ = 0;
    this.limEnv = 0;

    this.port.onmessage = (ev) => {
      const msg = ev.data;
      if (msg?.type === "reset") {
        this.prng = new XorShift32((msg.seed ?? 0) >>> 0);
        this.delay.fill(0);
        this.di = 0;
        this.jPhase = this.prng.nextFloat();
        this.jNoise = 0;
        this.errRemain = 0;
        this.errTotal = 0;
        this.lastGood = 0;
        this.errStart = 0;
        this.errEnd = 0;
        this.errEndSample = 0;
        this.ring.fill(0);
        this.ri = 0;
        this.clickRemain = 0;
        this.clickTotal = 0;
        this.clickAmp = 0;
        this.clickSign = 1;
        this.servoPhase = this.prng.nextFloat();
        this.servoPhase2 = this.prng.nextFloat();
        this.hfZ = 0;
        this.limEnv = 0;
      }
    };
  }

  _readDelay(delaySamps) {
    const len = this.delayLen;
    const read = this.di - delaySamps;
    const r0 = ((read | 0) % len + len) % len;
    const r1 = (r0 + 1) % len;
    const frac = read - Math.floor(read);
    return this.delay[r0] * (1 - frac) + this.delay[r1] * frac;
  }

  process(inputs, outputs, parameters) {
    const output = outputs[0];
    const out = output[0];
    if (!out) return true;
    const in0 = inputs[0]?.[0];
    const in1 = inputs[0]?.[1];

    const mode = Math.round(clamp(parameters.mode[0] ?? 0, 0, 3));
    const errorRate = clamp(parameters.errorRate[0] ?? 0.18, 0, 1);
    const burstMs = clamp(parameters.burstMs[0] ?? 24, 1, 600);
    const repeatMs = clamp(parameters.repeatMs[0] ?? 42, 1, 600);
    const scratchRate = clamp(parameters.scratchRate[0] ?? 0.25, 0, 1);
    const scratchAmt = clamp(parameters.scratchAmt[0] ?? 0.35, 0, 1);
    const jitterMs = clamp(parameters.jitterMs[0] ?? 0.18, 0, 3);
    const jitterRate = clamp(parameters.jitterRate[0] ?? 38, 1, 240);
    const hfLoss = clamp(parameters.hfLoss[0] ?? 0.1, 0, 1);
    const servoNoise = clamp(parameters.servoNoise[0] ?? 0.12, 0, 1);
    const doClip = (parameters.softClip[0] ?? 1) >= 0.5;
    const ceiling = clamp(parameters.ceiling[0] ?? 0.94, 0.2, 1);
    const outGain = clamp(parameters.outGain[0] ?? 0.98, 0, 1.5);

    const sr = sampleRate;
    const burstSamps = Math.max(1, Math.round((burstMs / 1000) * sr));
    const repeatSamps = Math.max(1, Math.round((repeatMs / 1000) * sr));

    // Jitter: tiny, relatively fast, plus some noise modulation.
    const depthSamps = (Math.min(1.6, jitterMs) / 1000) * sr; // <= ~1.6ms
    const jHz = 8 + jitterRate;
    const jNoiseAmt = 0.35 + 0.4 * errorRate;

    // Scratch clicks: event probability per sample.
    const clickP = (0.000005 + scratchRate * scratchRate * 0.0002) * (0.6 + 0.9 * errorRate);
    const clickLenMin = Math.max(2, Math.round((0.25 / 1000) * sr));
    const clickLenMax = Math.max(clickLenMin + 1, Math.round((4.0 / 1000) * sr));

    // Servo noise (whirr + chatter).
    const servo = servoNoise * servoNoise;
    const humHz = 120 + 60 * servo;
    const whirrHz = 420 + 280 * servo;
    const servoDepth = servo * 0.02;
    const chatterDepth = servo * 0.01;

    // HF loss: one-pole lowpass.
    const hfCut = 1800 + (1 - hfLoss) * 16000;
    const a = Math.exp((-2 * Math.PI * hfCut) / sr);

    const limAtk = Math.exp(-1 / (0.002 * sr));
    const limRel = Math.exp(-1 / (0.06 * sr));

    // Error bursts scheduler: probability per sample, scaled.
    const errP = (0.0000008 + errorRate * errorRate * 0.00005) * (1 + 2.5 * scratchRate);

    for (let i = 0; i < out.length; i++) {
      const x = in0 ? (in1 ? 0.5 * (in0[i] + in1[i]) : in0[i]) : 0;

      // Ring buffer always records the incoming stream (for repeat concealment).
      this.ring[this.ri] = x;
      this.ri = (this.ri + 1) % this.ringLen;

      // Jitter delay line: write then read with modulated delay.
      this.delay[this.di] = x;
      const n = this.prng.nextSigned();
      this.jNoise = this.jNoise * 0.995 + n * 0.005;
      this.jPhase += jHz / sr;
      if (this.jPhase >= 1) this.jPhase -= 1;
      const lfo = Math.sin(this.jPhase * 2 * Math.PI);
      const mod = (lfo + this.jNoise * jNoiseAmt) * 0.5;
      const d = Math.max(0, depthSamps * clamp(mod, -1, 1));
      let y = this._readDelay(d);
      this.di = (this.di + 1) % this.delayLen;

      // Start a new error burst?
      if (this.errRemain <= 0 && this.prng.nextFloat() < errP) {
        this.errRemain = burstSamps;
        this.errTotal = burstSamps;
        this.errStart = this.lastGood;
        // "End sample" is estimated from recent input; will smooth interpolation targets.
        this.errEndSample = x;
        this.errEnd = this.errEndSample;
      }

      // Concealment
      if (this.errRemain > 0) {
        const t = 1 - this.errRemain / this.errTotal;
        if (mode === 1) {
          // mute
          y = 0;
        } else if (mode === 2) {
          // interpolate
          const s0 = this.errStart;
          const s1 = this.errEnd;
          y = s0 + (s1 - s0) * t;
        } else if (mode === 3) {
          // repeat frames from ring buffer
          const off = repeatSamps;
          const read = (this.ri - off + this.ringLen) % this.ringLen;
          y = this.ring[read];
        } else {
          // hold
          y = this.lastGood;
        }
        this.errRemain--;
      } else {
        this.lastGood = y;
      }

      // Scratch/click injection (short clipped transient).
      if (this.clickRemain <= 0 && scratchAmt > 0.0001 && this.prng.nextFloat() < clickP) {
        const len = clickLenMin + Math.floor(this.prng.nextFloat() * (clickLenMax - clickLenMin));
        this.clickTotal = len;
        this.clickRemain = len;
        const a0 = 0.08 + 0.55 * scratchAmt;
        this.clickAmp = a0 * (0.55 + 0.9 * this.prng.nextFloat());
        this.clickSign = this.prng.nextFloat() < 0.5 ? -1 : 1;
      }
      if (this.clickRemain > 0) {
        const t = 1 - this.clickRemain / this.clickTotal;
        const env = Math.pow(1 - t, 2.8);
        const edge = t < 0.1 ? t / 0.1 : 1;
        const c = this.clickSign * this.clickAmp * env * edge;
        y = clamp(y + c, -1.2, 1.2);
        this.clickRemain--;
      }

      // Servo noise (very subtle).
      if (servo > 0.0001) {
        this.servoPhase += humHz / sr;
        this.servoPhase2 += whirrHz / sr;
        if (this.servoPhase >= 1) this.servoPhase -= 1;
        if (this.servoPhase2 >= 1) this.servoPhase2 -= 1;
        const hum = Math.sin(this.servoPhase * 2 * Math.PI) * servoDepth;
        const whirr = Math.sin(this.servoPhase2 * 2 * Math.PI) * servoDepth * 0.65;
        const chatter = this.prng.nextSigned() * chatterDepth;
        y = y + hum + whirr + chatter;
      }

      // HF loss filter.
      this.hfZ = (1 - a) * y + a * this.hfZ;
      y = this.hfZ;

      // Soft clip (optional), then output gain.
      let post = y * outGain;
      if (doClip) post = softClip(post);

      // Ceiling limiter.
      const aa = Math.abs(post);
      const lc = aa > this.limEnv ? limAtk : limRel;
      this.limEnv = aa + lc * (this.limEnv - aa);
      const g = this.limEnv > ceiling ? ceiling / (this.limEnv + 1e-6) : 1;
      post *= g;
      out[i] = clamp(post, -ceiling, ceiling);
    }

    return true;
  }
}

registerProcessor("cd", CdProcessor);

