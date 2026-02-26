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
      { name: "mix", defaultValue: 0.18, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "rate", defaultValue: 3, minValue: 0, maxValue: 18, automationRate: "k-rate" },
      { name: "wave", defaultValue: 0, minValue: 0, maxValue: 3, automationRate: "k-rate" }, // 0 random,1 pulse,2 saw,3 tri
      { name: "vibrato", defaultValue: 0.35, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "pitch", defaultValue: 0.55, minValue: 0, maxValue: 1, automationRate: "k-rate" },
    ];
  }
  constructor(options) {
    super();
    const seed = (options?.processorOptions?.seed ?? 0x0b1ee0f5) >>> 0;
    this.prng = new XorShift32(seed);
    this.samplesToNext = 0;
    this.phase = 0;
    this.vibPhase = 0;
    this.active = false;
    this.remain = 0;
    this.total = 0;
    this.freq = 440;
    this.vibRate = 6;
    this.vibDepth = 0;
    this.wave = 1;
    this.duty = 0.5;
    this.amp = 0.25;
    this.port.onmessage = (ev) => {
      const msg = ev.data;
      if (msg?.type === "reset") {
        this.prng = new XorShift32((msg.seed ?? 0) >>> 0);
        this.samplesToNext = 0;
        this.phase = 0;
        this.vibPhase = 0;
        this.active = false;
        this.remain = 0;
      }
    };
  }
  _trigger(pitch, vibrato, waveSel) {
    const baseLo = 220 + pitch * 320;
    const baseHi = 700 + pitch * 1800;
    const f = baseLo + this.prng.nextFloat() * (baseHi - baseLo);
    const durMs = 35 + this.prng.nextFloat() * (55 + pitch * 120);
    this.total = Math.max(8, Math.floor((durMs / 1000) * sampleRate));
    this.remain = this.total;
    this.active = true;
    this.phase = this.prng.nextFloat();
    this.vibPhase = this.prng.nextFloat();
    this.freq = f;

    const vibChance = Math.min(0.85, 0.15 + vibrato * 0.75);
    const doVib = this.prng.nextFloat() < vibChance;
    this.vibRate = 4 + this.prng.nextFloat() * 7;
    this.vibDepth = doVib ? (0.003 + 0.02 * vibrato * vibrato) * (0.6 + 0.6 * this.prng.nextFloat()) : 0;
    this.amp = (0.12 + 0.32 * this.prng.nextFloat()) * (0.55 + 0.6 * pitch);

    if (waveSel === 0) {
      const r = this.prng.nextFloat();
      this.wave = r < 0.45 ? 1 : r < 0.75 ? 3 : 2;
    } else this.wave = waveSel;
    this.duty = 0.25 + this.prng.nextFloat() * 0.55;
  }
  _env(t) {
    const a = Math.min(1, t / 0.12);
    const b = Math.min(1, (1 - t) / 0.2);
    return Math.sin(a * Math.PI * 0.5) * Math.sin(b * Math.PI * 0.5);
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
    // pulse
    return (phase - Math.floor(phase)) < this.duty ? 1 : -1;
  }
  process(inputs, outputs, parameters) {
    const output = outputs[0];
    const out = output[0];
    if (!out) return true;
    const enable = (parameters.enable[0] ?? 0) >= 0.5;
    const mix = clamp(parameters.mix[0] ?? 0.18, 0, 1);
    const rate = Math.max(0, parameters.rate[0] ?? 3);
    const waveSel = Math.round(parameters.wave[0] ?? 0);
    const vibrato = clamp(parameters.vibrato[0] ?? 0.35, 0, 1);
    const pitch = clamp(parameters.pitch[0] ?? 0.55, 0, 1);

    // Keep output silent when disabled.
    if (!enable || mix <= 0.0001 || rate <= 0.0001) {
      for (let i = 0; i < out.length; i++) out[i] = 0;
      return true;
    }

    const pPerSample = (0.15 + rate) / sampleRate;
    for (let i = 0; i < out.length; i++) {
      if (!this.active) {
        if (this.prng.nextFloat() < pPerSample) this._trigger(pitch, vibrato, waveSel);
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
        const edge = (this.wave === 1 ? 0.85 : 0.65) + vibrato * 0.15;
        y = softClip(osc * (this.amp * 2.6) * env * edge);
        this.remain--;
        if (this.remain <= 0) this.active = false;
      }

      out[i] = y * mix;
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

    const satAmt = 1 + sat * 10;
    const preEdge = 1 + edge * 18;
    const edgeAsym = edge * 0.22;
    const humHz = 50 + (this.prng.nextFloat() < 0.5 ? 10 : 0);
    const whineHz = 900 + 600 * whine;
    const humDepth = hum * 0.08;
    const whineDepth = whine * 0.075;
    const hissBase = noise * 0.03;

    const envAtk = Math.exp(-1 / (sampleRate * 0.004));
    const envRel = Math.exp(-1 / (sampleRate * 0.08));
    const limAtk = Math.exp(-1 / (sampleRate * 0.0015));
    const limRel = Math.exp(-1 / (sampleRate * 0.05));
    const dcStep = (0.000002 + dcDrift * dcDrift * 0.00003);

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
        this.dc = clamp(this.dc + this.prng.nextSigned() * dcStep, -0.08, 0.08);
        y = y + this.dc;
      }

      // Optional block hold (fake ADPCM blockiness): hold signal constant for small blocks.
      if (blockSamples > 0) {
        if (this.blockRemain <= 0) {
          this.blockTotal = blockSamples;
          this.blockRemain = this.blockTotal;
          this.blockHold = y;
        }
        // Slight drift to avoid pure gating.
        const t = 1 - this.blockRemain / this.blockTotal;
        y = this.blockHold + (y - this.blockHold) * (0.25 + 0.75 * t);
        this.blockRemain--;
      }

      // Pre-edge nonlinearity BEFORE downsampling to get authentic aliasy grit.
      if (edge > 0.0001) {
        y = softClip((y + edgeAsym) * preEdge) - edgeAsym * 0.6;
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
      if (mulaw > 0.0001) {
        const enc = mulawEncode(y);
        const dec = mulawDecode(enc);
        y = y * (1 - mulaw) + dec * mulaw;
      }

      // Quantization with optional dither + simple noise shaping.
      if (noiseShaping) y += this.nsErr * 0.85;
      if (dither) y += this.prng.nextSigned() * (qStep * 0.65);
      const q = Math.round(y * qLevels) / qLevels;
      if (noiseShaping) this.nsErr = y - q;
      y = q;

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
      const humSig = Math.sin(this.humPhase) * humDepth;
      const whineSig = Math.sin(this.whinePhase) * whineDepth;
      const hissSig = this.prng.nextSigned() * hiss;

      y = y + humSig + whineSig + hissSig;

      // DAC saturation.
      y = softClip(y * satAmt) / softClip(satAmt);

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
