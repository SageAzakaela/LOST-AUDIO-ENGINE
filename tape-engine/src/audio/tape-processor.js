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

class TapeProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: "speed", defaultValue: 1, minValue: 0.5, maxValue: 2, automationRate: "k-rate" },
      { name: "wowDepthMs", defaultValue: 3.5, minValue: 0, maxValue: 20, automationRate: "k-rate" },
      { name: "flutterDepthMs", defaultValue: 1.2, minValue: 0, maxValue: 10, automationRate: "k-rate" },
      { name: "wowAmount", defaultValue: 0.25, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "drive", defaultValue: 0.35, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "comp", defaultValue: 0.28, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "hiss", defaultValue: 0.12, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "hum", defaultValue: 0.05, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "dropout", defaultValue: 0.18, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "dropoutMs", defaultValue: 38, minValue: 1, maxValue: 400, automationRate: "k-rate" },
      { name: "ceiling", defaultValue: 0.92, minValue: 0.2, maxValue: 1, automationRate: "k-rate" },
      { name: "outGain", defaultValue: 0.98, minValue: 0, maxValue: 1.5, automationRate: "k-rate" },
    ];
  }

  constructor(options) {
    super();
    const seed = (options?.processorOptions?.seed ?? 0x74617065) >>> 0;
    this.prng = new XorShift32(seed);

    const maxDelayS = 0.06;
    this.delay = new Float32Array(Math.max(128, Math.ceil(sampleRate * maxDelayS)));
    this.di = 0;
    this.wowPhase = this.prng.nextFloat();
    this.flutterPhase = this.prng.nextFloat();
    this.drift = 0;

    this.env = 0;
    this.limEnv = 0;

    this.humPhase = 0;
    this.hissZ = 0;

    this.dropRemain = 0;
    this.dropBlock = 0;
    this.dropGain = 1;
    this.dropTarget = 1;

    this.port.onmessage = (ev) => {
      const msg = ev.data;
      if (msg?.type === "reset") {
        this.prng = new XorShift32((msg.seed ?? 0) >>> 0);
        this.di = 0;
        this.wowPhase = this.prng.nextFloat();
        this.flutterPhase = this.prng.nextFloat();
        this.drift = 0;
        this.env = 0;
        this.limEnv = 0;
        this.humPhase = 0;
        this.hissZ = 0;
        this.dropRemain = 0;
        this.dropBlock = 0;
        this.dropGain = 1;
        this.dropTarget = 1;
        this.delay.fill(0);
      }
    };
  }

  _readDelay(delaySamps) {
    const len = this.delay.length;
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

    const speed = clamp(parameters.speed[0] ?? 1, 0.5, 2);
    const wowDepthMs = clamp(parameters.wowDepthMs[0] ?? 3.5, 0, 20);
    const flutterDepthMs = clamp(parameters.flutterDepthMs[0] ?? 1.2, 0, 10);
    const wowAmount = clamp(parameters.wowAmount[0] ?? 0.25, 0, 1);
    const drive = clamp(parameters.drive[0] ?? 0.35, 0, 1);
    const comp = clamp(parameters.comp[0] ?? 0.28, 0, 1);
    const hiss = clamp(parameters.hiss[0] ?? 0.12, 0, 1);
    const hum = clamp(parameters.hum[0] ?? 0.05, 0, 1);
    const dropout = clamp(parameters.dropout[0] ?? 0.18, 0, 1);
    const dropoutMs = clamp(parameters.dropoutMs[0] ?? 38, 1, 400);
    const ceiling = clamp(parameters.ceiling[0] ?? 0.92, 0.2, 1);
    const outGain = clamp(parameters.outGain[0] ?? 0.98, 0, 1.5);

    const sr = sampleRate;

    // LFOs: wow (slow) + flutter (faster), plus slight random drift.
    const wowHz = 0.22 + wowAmount * 0.55;
    const flutterHz = 4.8 + wowAmount * 6.5;
    const baseDelayS = 0.012;
    const wowDepthS = (wowDepthMs / 1000) * (0.25 + 0.75 * wowAmount);
    const flutterDepthS = (flutterDepthMs / 1000) * (0.25 + 0.75 * wowAmount);
    const depthS = clamp(wowDepthS + flutterDepthS, 0, 0.03);

    // Downward tape compression. Tape should soften material that pushes above
    // its operating level; it must not behave like an AGC that drags quiet
    // passages, transport beds, and the noise floor upward.
    const target = 0.2;
    const envAtk = Math.exp(-1 / (0.006 * sr));
    const envRel = Math.exp(-1 / (0.12 * sr));
    const compPow = comp * 0.72;

    // Dropouts scheduler
    const blockSamples = Math.max(8, Math.round((dropoutMs / 1000) * sr));
    const fade = 48;
    const slew = 1 / fade;
    if (this.dropBlock <= 0) this.dropBlock = blockSamples;

    // Noise
    const humHz = 60;
    const humDepth = hum * hum * 0.02;
    const hissDepth = hiss * hiss * 0.03;

    // Drive
    const satAmt = 1 + drive * 6;
    const asym = drive * 0.045;
    const satZero = Math.tanh(asym * satAmt);
    const satSlope = Math.max(0.2, satAmt * (1 - satZero * satZero));

    // Limiter
    const limAtk = Math.exp(-1 / (0.002 * sr));
    const limRel = Math.exp(-1 / (0.06 * sr));

    for (let i = 0; i < out.length; i++) {
      const x0 = in0 ? (in1 ? 0.5 * (in0[i] + in1[i]) : in0[i]) : 0;

      // Write input to delay line.
      this.delay[this.di] = x0;

      // Drift: very slow random walk to avoid perfect periodicity.
      const driftStep = (wowAmount * wowAmount) * (2.2e-6);
      if (driftStep > 0) this.drift = clamp(this.drift + this.prng.nextSigned() * driftStep, -0.0018, 0.0018);

      const w = Math.sin(this.wowPhase * 2 * Math.PI);
      const f = Math.sin(this.flutterPhase * 2 * Math.PI);
      const modS = (w * wowDepthS + f * flutterDepthS + this.drift) * (0.6 + 0.4 * wowAmount);
      const delayS = clamp(baseDelayS + modS, 0.001, baseDelayS + depthS + 0.01);
      const delaySamps = delayS * sr;

      // Read modulated delay, advance phases.
      let y = this._readDelay(delaySamps);
      this.wowPhase += (wowHz * speed) / sr;
      this.flutterPhase += (flutterHz * speed) / sr;
      if (this.wowPhase >= 1) this.wowPhase -= 1;
      if (this.flutterPhase >= 1) this.flutterPhase -= 1;

      // Level-dependent tape compression. At zero the stage is unity instead of
      // silently applying automatic gain to every preset.
      const a = Math.abs(y);
      const c = a > this.env ? envAtk : envRel;
      this.env = a + c * (this.env - a);
      const env = this.env + 1e-6;
      const over = Math.max(1, env / target);
      const compressionGain = comp > 0.0001 ? Math.pow(over, -compPow) : 1;
      const makeupGain = 1 + comp * 0.12;
      y *= Math.max(0.45, compressionGain) * makeupGain;

      // Gain-compensated saturation with a zero-referenced bias. The previous
      // bias subtraction left a large DC-like signal even when the input was silent.
      const saturated = (softClip((y + asym) * satAmt) - satZero) / satSlope;
      y = y * (1 - drive) + saturated * drive;

      // Dropouts in blocks.
      if (this.dropBlock <= 0) {
        this.dropBlock = blockSamples;
        const p = dropout * dropout;
        const doDrop = this.prng.nextFloat() < p;
        this.dropRemain = doDrop ? blockSamples : 0;
      }
      this.dropBlock--;
      this.dropTarget = this.dropRemain > 0 ? 0 : 1;
      this.dropGain = clamp(this.dropGain + (this.dropTarget - this.dropGain) * slew, 0, 1);
      if (this.dropRemain > 0) this.dropRemain--;
      y *= this.dropGain;

      // Hum + hiss (hiss highpassed by a 1st order differentiator).
      this.humPhase += (2 * Math.PI * humHz) / sr;
      if (this.humPhase > Math.PI * 2) this.humPhase -= Math.PI * 2;
      const humSig = Math.sin(this.humPhase) * humDepth;
      const wn = this.prng.nextSigned();
      const hp = wn - this.hissZ;
      this.hissZ = wn;
      const hissSig = hp * hissDepth;
      y = y + humSig + hissSig;

      // Output gain + limiter to ceiling.
      let post = y * outGain;
      const aa = Math.abs(post);
      const lc = aa > this.limEnv ? limAtk : limRel;
      this.limEnv = aa + lc * (this.limEnv - aa);
      const g = this.limEnv > ceiling ? ceiling / (this.limEnv + 1e-6) : 1;
      post *= g;
      out[i] = clamp(post, -ceiling, ceiling);

      // Advance delay index.
      this.di++;
      if (this.di >= this.delay.length) this.di = 0;
    }

    return true;
  }
}

registerProcessor("tape", TapeProcessor);

