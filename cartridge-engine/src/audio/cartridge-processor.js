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

function mulawEncode(x, mu = 255) {
  const s = Math.sign(x);
  const ax = Math.min(1, Math.abs(x));
  const y = Math.log1p(mu * ax) / Math.log1p(mu);
  return s * y;
}
function mulawDecode(y, mu = 255) {
  const s = Math.sign(y);
  const ay = Math.min(1, Math.abs(y));
  const x = (Math.expm1(ay * Math.log1p(mu))) / mu;
  return s * x;
}

class BleepSeqProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: "enable", defaultValue: 0, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "mix", defaultValue: 0.12, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "rate", defaultValue: 3, minValue: 0, maxValue: 18, automationRate: "k-rate" },
      { name: "wave", defaultValue: 1, minValue: 0, maxValue: 4, automationRate: "k-rate" }, // 0 alternating,1 pulse,2 saw,3 tri,4 noise
      { name: "trigger", defaultValue: 0, minValue: 0, maxValue: 2, automationRate: "k-rate" }, // 0 transients,1 clock,2 hybrid
      { name: "scale", defaultValue: 0, minValue: 0, maxValue: 3, automationRate: "k-rate" },
      { name: "vibrato", defaultValue: 0.12, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "pitch", defaultValue: 0.55, minValue: 0, maxValue: 1, automationRate: "k-rate" },
    ];
  }
  constructor(options) {
    super();
    const seed = (options?.processorOptions?.seed ?? 0x0b1ee0f5) >>> 0;
    this.prng = new XorShift32(seed);
    this.inputEnv = 0;
    this.cooldown = 0;
    this.clockRemain = 0;
    this.step = 0;
    this.phase = 0;
    this.vibPhase = 0;
    this.active = false;
    this.remain = 0;
    this.total = 0;
    this.freq = 440;
    this.vibRate = 5;
    this.vibDepth = 0;
    this.wave = 1;
    this.duty = 0.25;
    this.amp = 0.16;
    this.lfsr = 0x7fff;
    this.port.onmessage = (ev) => {
      const msg = ev.data;
      if (msg?.type === "reset") {
        this.prng = new XorShift32((msg.seed ?? 0) >>> 0);
        this.inputEnv = 0;
        this.cooldown = 0;
        this.clockRemain = 0;
        this.step = 0;
        this.phase = 0;
        this.vibPhase = 0;
        this.active = false;
        this.remain = 0;
      }
    };
  }
  _trigger(pitch, vibrato, waveSel, scaleSel, strength = 1) {
    const scales = [
      [0, 3, 5, 7, 10, 12],
      [0, 2, 3, 5, 7, 8, 10, 12],
      [0, 2, 4, 5, 7, 9, 11, 12],
      [0, 1, 2, 3, 4, 5, 7, 9, 10, 12],
    ];
    const melody = [0, 4, 2, 5, 1, 3, 2, 0, 5, 3, 1, 4];
    const scale = scales[Math.max(0, Math.min(scales.length - 1, scaleSel))];
    const rootMidi = 36 + Math.round(pitch * 24);
    const interval = scale[melody[this.step % melody.length] % scale.length];
    const octave = (Math.floor(this.step / melody.length) & 1) * 12;
    this.freq = 440 * Math.pow(2, (rootMidi + interval + octave - 69) / 12);
    this.step++;

    const durMs = 42 + (1 - pitch) * 72 + Math.min(1, strength) * 38;
    this.total = Math.max(8, Math.floor((durMs / 1000) * sampleRate));
    this.remain = this.total;
    this.active = true;
    this.phase = this.prng.nextFloat();
    this.vibPhase = this.prng.nextFloat();
    this.vibRate = 4.5 + (this.step % 3) * 0.75;
    this.vibDepth = 0.001 + vibrato * vibrato * 0.012;
    this.amp = 0.09 + Math.min(1, strength) * 0.1;

    if (waveSel === 0) {
      this.wave = this.step % 4 === 0 ? 3 : 1;
    } else this.wave = waveSel;
    const duties = [0.125, 0.25, 0.5];
    this.duty = duties[this.step % duties.length];
  }
  _env(t) {
    const attack = Math.min(1, t / 0.035);
    const decay = Math.pow(Math.max(0, 1 - t), 1.65);
    return attack * decay;
  }
  _osc(phase) {
    if (this.wave === 2) {
      // saw
      return 2 * (phase - Math.floor(phase + 0.5));
    }
    if (this.wave === 3) {
      // triangle
      const p = phase - Math.floor(phase);
      return 1 - 4 * Math.abs(p - 0.5);
    }
    if (this.wave === 4) {
      const bit = ((this.lfsr >> 0) ^ (this.lfsr >> 1)) & 1;
      this.lfsr = (this.lfsr >> 1) | (bit << 14);
      return (this.lfsr & 1) ? 1 : -1;
    }
    // pulse
    return (phase - Math.floor(phase)) < this.duty ? 1 : -1;
  }
  process(inputs, outputs, parameters) {
    const output = outputs[0];
    const out = output[0];
    if (!out) return true;
    const in0 = inputs[0]?.[0];
    const in1 = inputs[0]?.[1];
    const enable = (parameters.enable[0] ?? 0) >= 0.5;
    const mix = clamp(parameters.mix[0] ?? 0.12, 0, 1);
    const rate = Math.max(0, parameters.rate[0] ?? 3);
    const waveSel = Math.round(parameters.wave[0] ?? 0);
    const triggerMode = Math.round(clamp(parameters.trigger[0] ?? 0, 0, 2));
    const scaleSel = Math.round(clamp(parameters.scale[0] ?? 0, 0, 3));
    const vibrato = clamp(parameters.vibrato[0] ?? 0.12, 0, 1);
    const pitch = clamp(parameters.pitch[0] ?? 0.55, 0, 1);

    // Keep output silent when disabled.
    if (!enable || mix <= 0.0001 || rate <= 0.0001) {
      for (let i = 0; i < out.length; i++) out[i] = 0;
      return true;
    }

    const clockPeriod = Math.max(1, Math.round(sampleRate / Math.max(0.15, rate)));
    const envAtk = 1 - Math.exp(-1 / (0.0025 * sampleRate));
    const envRel = 1 - Math.exp(-1 / (0.055 * sampleRate));
    for (let i = 0; i < out.length; i++) {
      const source = in0 ? (in1 ? 0.5 * (in0[i] + in1[i]) : in0[i]) : 0;
      const a = Math.abs(source);
      const previousEnv = this.inputEnv;
      this.inputEnv += (a - this.inputEnv) * (a > this.inputEnv ? envAtk : envRel);
      if (this.cooldown > 0) this.cooldown--;
      if (this.clockRemain > 0) this.clockRemain--;

      const transient = a > 0.018 && a > previousEnv * 1.85 && this.cooldown <= 0;
      const clocked = this.clockRemain <= 0 && rate > 0.0001;
      const shouldTrigger = triggerMode === 0 ? transient : triggerMode === 1 ? clocked : transient || clocked;
      if (shouldTrigger) {
        this._trigger(pitch, vibrato, waveSel, scaleSel, clamp(a * 4 + 0.25, 0.25, 1));
        this.cooldown = Math.round(sampleRate / Math.max(1, rate * 1.5));
        this.clockRemain = clockPeriod;
      }

      let y = 0;
      if (this.active && this.remain > 0) {
        const t = 1 - this.remain / this.total;
        const env = this._env(t);
        const vib = this.vibDepth > 0 ? Math.sin(this.vibPhase * 2 * Math.PI) * this.vibDepth : 0;
        const f = this.freq * (1 + vib);
        this.phase += f / sampleRate;
        this.vibPhase += this.vibRate / sampleRate;
        const osc = this._osc(this.phase);
        y = osc * this.amp * env;
        this.remain--;
        if (this.remain <= 0) this.active = false;
      }

      out[i] = clamp(y * mix, -0.22, 0.22);
    }
    return true;
  }
}

class CartridgeProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: "bits", defaultValue: 10, minValue: 2, maxValue: 16, automationRate: "k-rate" },
      { name: "rate", defaultValue: 24000, minValue: 6000, maxValue: 48000, automationRate: "k-rate" },
      { name: "jitter", defaultValue: 0.05, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "dither", defaultValue: 1, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "noiseShaping", defaultValue: 0, minValue: 0, maxValue: 1, automationRate: "k-rate" },

      { name: "preEmph", defaultValue: 0.2, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "mulaw", defaultValue: 0.25, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "codecMode", defaultValue: 2, minValue: 0, maxValue: 4, automationRate: "k-rate" },
      { name: "blockMs", defaultValue: 8, minValue: 0, maxValue: 60, automationRate: "k-rate" },

      { name: "sat", defaultValue: 0.25, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "edge", defaultValue: 0.25, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "dcDrift", defaultValue: 0.15, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "hum", defaultValue: 0.08, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "whine", defaultValue: 0.15, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "noise", defaultValue: 0.15, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "noiseTrack", defaultValue: 0.6, minValue: 0, maxValue: 1, automationRate: "k-rate" },

      { name: "microDelayMs", defaultValue: 8, minValue: 0, maxValue: 30, automationRate: "k-rate" },
      { name: "microDelayMix", defaultValue: 0.18, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "verb", defaultValue: 0.22, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "verbMs", defaultValue: 45, minValue: 10, maxValue: 120, automationRate: "k-rate" },

      { name: "limiter", defaultValue: 0.35, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "ceiling", defaultValue: 0.92, minValue: 0.2, maxValue: 1, automationRate: "k-rate" },
      { name: "wet", defaultValue: 1, minValue: 0, maxValue: 1, automationRate: "k-rate" },

      { name: "outGain", defaultValue: 0.95, minValue: 0, maxValue: 1.5, automationRate: "k-rate" },
    ];
  }

  constructor(options) {
    super();
    const seed = (options?.processorOptions?.seed ?? 0xfeedc0de) >>> 0;
    this.prng = new XorShift32(seed);

    this.hold = 0;
    this.holdCount = 0;
    this.holdPeriod = 1;

    this.blockHold = 0;
    this.blockRemain = 0;
    this.blockTotal = 0;

    this.preEmphZ = 0;
    this.deEmphZ = 0;
    this.codecPrev1 = 0;
    this.codecPrev2 = 0;
    this.adpcmStep = 0.02;

    this.nsErr = 0;
    this.env = 0;
    this.dc = 0;
    this.humPhase = 0;
    this.whinePhase = 0;
    this.limEnv = 0;

    this.delayBuf = new Float32Array(Math.max(1, Math.floor(sampleRate * 0.05)));
    this.delayIndex = 0;

    this.c1 = new Float32Array(Math.max(1, Math.floor(sampleRate * 0.14)));
    this.c2 = new Float32Array(Math.max(1, Math.floor(sampleRate * 0.14)));
    this.c3 = new Float32Array(Math.max(1, Math.floor(sampleRate * 0.14)));
    this.ci1 = 0;
    this.ci2 = 0;
    this.ci3 = 0;
    this.c1lp = 0;
    this.c2lp = 0;
    this.c3lp = 0;
    this.ap1 = new Float32Array(Math.max(1, Math.floor(sampleRate * 0.06)));
    this.ap2 = new Float32Array(Math.max(1, Math.floor(sampleRate * 0.06)));
    this.api1 = 0;
    this.api2 = 0;

    this.port.onmessage = (ev) => {
      const msg = ev.data;
      if (msg?.type === "reset") {
        this.prng = new XorShift32((msg.seed ?? 0) >>> 0);
        this.hold = 0;
        this.holdCount = 0;
        this.blockHold = 0;
        this.blockRemain = 0;
        this.preEmphZ = 0;
        this.deEmphZ = 0;
        this.codecPrev1 = 0;
        this.codecPrev2 = 0;
        this.adpcmStep = 0.02;
        this.nsErr = 0;
        this.env = 0;
        this.dc = 0;
        this.humPhase = 0;
        this.whinePhase = 0;
        this.limEnv = 0;

        this.delayBuf.fill(0);
        this.delayIndex = 0;
        this.c1.fill(0);
        this.c2.fill(0);
        this.c3.fill(0);
        this.ci1 = this.ci2 = this.ci3 = 0;
        this.c1lp = this.c2lp = this.c3lp = 0;
        this.ap1.fill(0);
        this.ap2.fill(0);
        this.api1 = this.api2 = 0;
      }
    };
  }

  _comb(buf, idx, input, delaySamps, fb, damp, lpState) {
    const len = buf.length;
    const read = (idx - delaySamps + len) % len;
    const y = buf[read];
    const lp = y + damp * (lpState - y);
    buf[idx] = input + lp * fb;
    return { out: y, lp };
  }

  _allpass(buf, idx, input, delaySamps, g) {
    const len = buf.length;
    const read = (idx - delaySamps + len) % len;
    const b = buf[read];
    const y = -g * input + b;
    buf[idx] = input + y * g;
    return y;
  }

  process(inputs, outputs, parameters) {
    const input = inputs[0];
    const output = outputs[0];
    const out = output[0];
    const in0 = input?.[0];
    const in1 = input?.[1];
    if (!out) return true;

    const bits = Math.round(parameters.bits[0] ?? 10);
    const targetRate = parameters.rate[0] ?? 24000;
    const jitter = clamp(parameters.jitter[0] ?? 0.05, 0, 1);
    const dither = (parameters.dither[0] ?? 1) >= 0.5;
    const noiseShaping = (parameters.noiseShaping[0] ?? 0) >= 0.5;

    const preEmph = clamp(parameters.preEmph[0] ?? 0.2, 0, 1);
    const mulaw = clamp(parameters.mulaw[0] ?? 0.25, 0, 1);
    const codecMode = Math.round(clamp(parameters.codecMode[0] ?? 2, 0, 4));
    const blockMs = Math.max(0, parameters.blockMs[0] ?? 8);

    const sat = clamp(parameters.sat[0] ?? 0.25, 0, 1);
    const edge = clamp(parameters.edge[0] ?? 0.25, 0, 1);
    const dcDrift = clamp(parameters.dcDrift[0] ?? 0.15, 0, 1);
    const hum = clamp(parameters.hum[0] ?? 0.08, 0, 1);
    const whine = clamp(parameters.whine[0] ?? 0.15, 0, 1);
    const noise = clamp(parameters.noise[0] ?? 0.15, 0, 1);
    const noiseTrack = clamp(parameters.noiseTrack[0] ?? 0.6, 0, 1);
    const microDelayMs = Math.max(0, parameters.microDelayMs[0] ?? 8);
    const microDelayMix = clamp(parameters.microDelayMix[0] ?? 0.18, 0, 1);
    const verb = clamp(parameters.verb[0] ?? 0.22, 0, 1);
    const verbMs = Math.max(10, Math.min(120, parameters.verbMs[0] ?? 45));
    const limiter = clamp(parameters.limiter[0] ?? 0.35, 0, 1);
    const ceiling = clamp(parameters.ceiling[0] ?? 0.92, 0.2, 1);
    const wet = clamp(parameters.wet[0] ?? 1, 0, 1);
    const outGain = parameters.outGain[0] ?? 0.95;

    const holdRate = clamp(targetRate, 6000, sampleRate);
    const basePeriod = Math.max(1, Math.round(sampleRate / holdRate));
    const qLevels = Math.pow(2, Math.max(1, bits - 1));
    const qStep = 1 / qLevels;

    const blockSamples = blockMs <= 0 ? 0 : Math.max(1, Math.floor((blockMs / 1000) * sampleRate));

    const satAmt = 1 + sat * 4.2;
    const satRef = 0.2;
    const satMatch = satRef / Math.max(1e-5, softClip(satRef * satAmt));
    const preEdge = 1 + edge * 5;
    const edgeAsym = edge * 0.035;
    const edgeRef = 0.18;
    const edgeMatch = edgeRef / Math.max(1e-5, softClip(edgeRef * preEdge));
    const humHz = 60;
    const whineHz = 780 + 1180 * whine;
    const humDepth = hum * hum * 0.012;
    const whineDepth = whine * whine * 0.009;
    const hissBase = noise * noise * 0.012;

    const envAtk = Math.exp(-1 / (sampleRate * 0.004));
    const envRel = Math.exp(-1 / (sampleRate * 0.08));
    const limAtk = Math.exp(-1 / (sampleRate * 0.0015));
    const limRel = Math.exp(-1 / (sampleRate * 0.05));
    const dcStep = dcDrift * dcDrift * 0.000003;

    const dSamps = Math.min(this.delayBuf.length - 1, Math.floor((microDelayMs / 1000) * sampleRate));
    const combBase = Math.floor((verbMs / 1000) * sampleRate);
    const d1 = Math.max(32, Math.min(this.c1.length - 1, Math.floor(combBase * 0.55)));
    const d2 = Math.max(32, Math.min(this.c2.length - 1, Math.floor(combBase * 0.78)));
    const d3 = Math.max(32, Math.min(this.c3.length - 1, Math.floor(combBase * 1.05)));
    const fb = 0.12 + verb * 0.72;
    const damp = 0.22 + verb * 0.55;
    const apD1 = Math.max(16, Math.min(this.ap1.length - 1, Math.floor(combBase * 0.33)));
    const apD2 = Math.max(16, Math.min(this.ap2.length - 1, Math.floor(combBase * 0.27)));
    const apG = 0.55;

    for (let i = 0; i < out.length; i++) {
      const x = in0 ? (in1 ? 0.5 * (in0[i] + in1[i]) : in0[i]) : 0;
      const dry0 = x;

      // Pre-emphasis: simple 1st order high-shelf-ish tilt (diff + bleed).
      const diff = x - this.preEmphZ;
      this.preEmphZ = x;
      let y = x + diff * (0.6 * preEmph);

      // DC drift (random walk) + coupling bias.
      if (dcDrift > 0.0001) {
        this.dc = clamp(this.dc * 0.999995 + this.prng.nextSigned() * dcStep, -0.018, 0.018);
        y = y + this.dc;
      }

      // Cartridge codecs reset predictors at small block boundaries. A block
      // boundary should never freeze the waveform itself.
      if (blockSamples > 0) {
        if (this.blockRemain <= 0) {
          this.blockTotal = blockSamples;
          this.blockRemain = this.blockTotal;
          this.codecPrev1 *= 0.2;
          this.codecPrev2 *= 0.2;
          this.adpcmStep = Math.max(0.004, Math.min(0.18, Math.abs(y) * 0.18 + 0.008));
        }
        this.blockRemain--;
      }

      // Pre-edge nonlinearity BEFORE downsampling to get authentic aliasy grit.
      if (edge > 0.0001) {
        const center = softClip(edgeAsym * preEdge);
        y = (softClip((y + edgeAsym) * preEdge) - center) * edgeMatch;
      }

      // Sample-rate reduction with jitter.
      if (this.holdCount <= 0) {
        this.hold = y;
        const j = (this.prng.nextSigned() * 0.45) * jitter;
        this.holdPeriod = Math.max(1, Math.round(basePeriod * (1 + j)));
        this.holdCount = this.holdPeriod;
      }
      y = this.hold;
      this.holdCount--;

      // μ-law companding (encode/decode), blended.
      if (mulaw > 0.0001 && codecMode === 4) {
        const enc = mulawEncode(y);
        // A bare encode/decode round trip is reversible. Quantize the encoded
        // value as an 8-bit companded signal so this control has a real effect.
        // Increase companded-domain damage with the amount control so the top
        // of the range reaches an intentionally exaggerated 6-bit code path.
        const encodedLevels = Math.max(31, Math.round(127 - mulaw * 96));
        const encodedQuantized = Math.round(enc * encodedLevels) / encodedLevels;
        const dec = mulawDecode(encodedQuantized);
        y = y * (1 - mulaw) + dec * mulaw;
      } else if (mulaw > 0.0001 && codecMode > 0) {
        let decoded = y;
        if (codecMode === 1) {
          const predictor = this.codecPrev1 * 0.86;
          const levels = Math.max(7, Math.round(63 - mulaw * 48));
          const residual = clamp(y - predictor, -1, 1);
          decoded = clamp(predictor + Math.round(residual * levels) / levels, -1, 1);
        } else {
          const predictor = codecMode === 3
            ? 1.48 * this.codecPrev1 - 0.52 * this.codecPrev2
            : this.codecPrev1 * 0.92;
          const residual = y - predictor;
          const code = clamp(Math.round(residual / Math.max(0.003, this.adpcmStep)), -7, 7);
          decoded = clamp(predictor + code * this.adpcmStep, -1, 1);
          const magnitude = Math.abs(code) / 7;
          const targetStep = 0.0035 + magnitude * magnitude * (codecMode === 3 ? 0.12 : 0.085);
          this.adpcmStep += (targetStep - this.adpcmStep) * (code === 0 ? 0.035 : 0.16);
        }
        this.codecPrev2 = this.codecPrev1;
        this.codecPrev1 = decoded;
        y = y * (1 - mulaw) + decoded * mulaw;
      }

      // Quantization with optional dither + simple noise shaping.
      if (noiseShaping) y += this.nsErr * 0.85;
      if (dither) y += this.prng.nextSigned() * (qStep * 0.65);
      const q = bits <= 3
        ? Math.sign(y) * (Math.floor(Math.abs(y) * qLevels) + 0.5) / qLevels
        : Math.round(y * qLevels) / qLevels;
      if (noiseShaping) this.nsErr = y - q;
      y = q;

      const deCoeff = 0.08 + (1 - preEmph) * 0.34;
      this.deEmphZ += (y - this.deEmphZ) * deCoeff;
      y = y * (1 - preEmph * 0.52) + this.deEmphZ * (preEmph * 0.52);

      // Track amplitude for bus-noise behavior.
      const a = Math.abs(y);
      const c = a > this.env ? envAtk : envRel;
      this.env = a + c * (this.env - a);
      const inv = 1 - clamp(this.env * 3.2, 0, 1);
      const hiss = hissBase * (1 - noiseTrack + noiseTrack * (0.35 + 0.65 * inv));

      // Add analog-ish junk: hum + whine + hiss.
      this.humPhase += (2 * Math.PI * humHz) / sampleRate;
      if (this.humPhase > Math.PI * 2) this.humPhase -= Math.PI * 2;
      this.whinePhase += (2 * Math.PI * whineHz) / sampleRate;
      if (this.whinePhase > Math.PI * 2) this.whinePhase -= Math.PI * 2;
      const humSig = (Math.sin(this.humPhase) + Math.sin(this.humPhase * 2) * 0.28) * humDepth;
      const whineSig = (Math.sin(this.whinePhase) + Math.sin(this.whinePhase * 0.503) * 0.18) * whineDepth;
      const hissSig = this.prng.nextSigned() * hiss;

      y = y + humSig + whineSig + hissSig;

      // DAC saturation.
      y = softClip(y * satAmt) * satMatch;

      // Microdelay (tiny smear).
      if (dSamps > 0 && microDelayMix > 0.0001) {
        const len = this.delayBuf.length;
        const read = (this.delayIndex - dSamps + len) % len;
        const d = this.delayBuf[read];
        this.delayBuf[this.delayIndex] = y;
        this.delayIndex = (this.delayIndex + 1) % len;
        y = y * (1 - microDelayMix) + d * microDelayMix;
      } else {
        this.delayBuf[this.delayIndex] = y;
        this.delayIndex = (this.delayIndex + 1) % this.delayBuf.length;
      }

      // Conduction reverb (very short comb + allpass diffusion).
      if (verb > 0.0001) {
        const c1r = this._comb(this.c1, this.ci1, y, d1, fb, damp, this.c1lp);
        this.c1lp = c1r.lp;
        const c2r = this._comb(this.c2, this.ci2, y, d2, fb, damp, this.c2lp);
        this.c2lp = c2r.lp;
        const c3r = this._comb(this.c3, this.ci3, y, d3, fb, damp, this.c3lp);
        this.c3lp = c3r.lp;
        this.ci1 = (this.ci1 + 1) % this.c1.length;
        this.ci2 = (this.ci2 + 1) % this.c2.length;
        this.ci3 = (this.ci3 + 1) % this.c3.length;
        let rv = (c1r.out + c2r.out + c3r.out) * 0.33;
        rv = this._allpass(this.ap1, this.api1, rv, apD1, apG);
        this.api1 = (this.api1 + 1) % this.ap1.length;
        rv = this._allpass(this.ap2, this.api2, rv, apD2, apG);
        this.api2 = (this.api2 + 1) % this.ap2.length;
        y = y * (1 - verb * 0.65) + rv * (verb * 0.65);
      }

      // Limiter/ceiling at the end (keeps grit aggressive without ugly output clipping).
      const post = y * outGain;
      const aa = Math.abs(post);
      const lc = aa > this.limEnv ? limAtk : limRel;
      this.limEnv = aa + lc * (this.limEnv - aa);
      const thr = ceiling;
      const want = this.limEnv > thr ? thr / (this.limEnv + 1e-6) : 1;
      const g = 1 - limiter + limiter * want;
      const limited = clamp(post * g, -thr, thr);

      const wetOut = limited;
      out[i] = clamp(dry0 * (1 - wet) + wetOut * wet, -1, 1);
    }

    return true;
  }
}

registerProcessor("bleep-seq", BleepSeqProcessor);
registerProcessor("cartridge", CartridgeProcessor);
