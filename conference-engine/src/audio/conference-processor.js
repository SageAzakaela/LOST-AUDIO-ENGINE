/* eslint-disable no-undef */
class XorShift32 {
  constructor(seed) { this.state = (seed >>> 0) || 0x12345678; }
  nextU32() {
    let x = this.state >>> 0;
    x ^= x << 13; x ^= x >>> 17; x ^= x << 5;
    this.state = x >>> 0;
    return this.state;
  }
  nextFloat() { return (this.nextU32() >>> 0) / 0xffffffff; }
  nextSigned() { return this.nextFloat() * 2 - 1; }
}

function clamp(x, a, b) { return Math.min(b, Math.max(a, x)); }
function clamp01(x) { return Math.min(1, Math.max(0, x)); }

// Model a decoded speech stream, not a broken sound card. The important
// failures happen on codec frames: burst loss, PLC, jitter-buffer slips,
// DTX/noise suppression, and short repeated fragments.
class ConferenceProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: "mode", defaultValue: 0, minValue: 0, maxValue: 3, automationRate: "k-rate" },
      { name: "concealMode", defaultValue: 0, minValue: 0, maxValue: 3, automationRate: "k-rate" },
      { name: "packetLoss", defaultValue: 0.045, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "packetMs", defaultValue: 20, minValue: 4, maxValue: 240, automationRate: "k-rate" },
      { name: "repeatMs", defaultValue: 38, minValue: 1, maxValue: 300, automationRate: "k-rate" },
      { name: "jitterMs", defaultValue: 0.35, minValue: 0, maxValue: 20, automationRate: "k-rate" },
      { name: "jitterRate", defaultValue: 18, minValue: 1, maxValue: 220, automationRate: "k-rate" },
      { name: "gate", defaultValue: 0.12, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "bits", defaultValue: 12, minValue: 4, maxValue: 16, automationRate: "k-rate" },
      { name: "rate", defaultValue: 24000, minValue: 6000, maxValue: 48000, automationRate: "k-rate" },
      { name: "robot", defaultValue: 0.12, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "noise", defaultValue: 0.12, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "burstiness", defaultValue: 0.56, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "suppression", defaultValue: 0.42, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "agc", defaultValue: 0.34, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "bufferSlip", defaultValue: 0.08, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "bandwidthSwitch", defaultValue: 0.12, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "comfortNoise", defaultValue: 0.22, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "ceiling", defaultValue: 0.92, minValue: 0.2, maxValue: 1, automationRate: "k-rate" },
      { name: "outGain", defaultValue: 0.98, minValue: 0, maxValue: 1.5, automationRate: "k-rate" },
    ];
  }

  constructor(options) {
    super();
    this.initialSeed = (options?.processorOptions?.seed ?? 0x636f6e66) >>> 0;
    this.historyLen = Math.max(4096, Math.ceil(sampleRate * 0.8));
    this.history = new Float32Array(this.historyLen);
    this.inputDelay = new Float32Array(Math.max(1024, Math.ceil(sampleRate * 0.04)));
    this.reset(this.initialSeed);
    this.port.onmessage = (ev) => {
      if (ev.data?.type === "reset") this.reset((ev.data.seed ?? this.initialSeed) >>> 0);
    };
  }

  reset(seed) {
    this.prng = new XorShift32(seed);
    this.history.fill(0); this.inputDelay.fill(0);
    this.hi = 0; this.di = 0;
    this.frameRemain = 0; this.frameTotal = 1; this.frameIndex = 0;
    this.lossState = false; this.lossFrames = 0; this.lossAnchor = 0; this.plcPhase = 0;
    this.lastConcealed = 0; this.recoverRemain = 0; this.recoverTotal = 1;
    this.slipRemain = 0; this.slipAnchor = 0; this.slipLength = 1; this.slipPhase = 0;
    this.robotRemain = 0; this.robotAnchor = 0; this.robotLength = 1; this.robotPhase = 0;
    this.narrowFrames = 0; this.jitterTarget = 1; this.jitterSmooth = 1;
    this.env = 0; this.speechGain = 1; this.agcGain = 1;
    this.codecGain = 1; this.codecGainTarget = 1; this.codecLp = 0; this.codecHpLp = 0;
    this.comfort = 0; this.ratePhase = 0; this.rateHold = 0; this.smearA = 0; this.smearB = 0;
  }

  _readCircular(buf, writeIndex, back) {
    const len = buf.length;
    const read = writeIndex - Math.max(1, back);
    const base = Math.floor(read);
    const frac = read - base;
    const i0 = ((base % len) + len) % len;
    const i1 = (i0 + 1) % len;
    return buf[i0] * (1 - frac) + buf[i1] * frac;
  }
  _historyAt(index) {
    return this.history[((index % this.historyLen) + this.historyLen) % this.historyLen];
  }

  _beginFrame(cfg) {
    const wasLost = this.lossState;
    const stayBad = 0.08 + cfg.burstiness * 0.86;
    const enterBad = clamp01(cfg.packetLoss * (1 - stayBad) / Math.max(0.04, 1 - cfg.packetLoss));
    this.lossState = wasLost ? this.prng.nextFloat() < stayBad : this.prng.nextFloat() < enterBad;
    if (this.lossState) {
      if (!wasLost) { this.lossAnchor = this.hi; this.plcPhase = 0; }
      this.lossFrames++;
    } else {
      if (wasLost) {
        this.recoverTotal = Math.max(8, Math.round(sampleRate * (0.0025 + 0.004 * cfg.packetLoss)));
        this.recoverRemain = this.recoverTotal;
      }
      this.lossFrames = 0;
    }

    // Jitter buffers duplicate/skip chunks; they do not continuously chorus.
    const jitterNorm = clamp01(cfg.jitterMs / 12);
    const slipChance = clamp01((cfg.bufferSlip * 0.24 + jitterNorm * 0.12) * (0.35 + cfg.jitterRate / 38));
    if (!this.lossState && this.slipRemain <= 0 && this.prng.nextFloat() < slipChance) {
      this.slipLength = Math.max(8, Math.round(this.frameTotal * (0.45 + this.prng.nextFloat() * 0.85)));
      this.slipRemain = this.slipLength; this.slipPhase = 0;
      const duplicate = this.prng.nextFloat() < 0.72;
      this.slipAnchor = this.hi - (duplicate ? this.frameTotal : Math.round(this.frameTotal * 0.2));
    }
    this.jitterTarget = 1 + this.prng.nextFloat() * (cfg.jitterMs / 1000) * sampleRate;

    const collapseChance = cfg.bandwidthSwitch * (0.025 + 0.1 * cfg.packetLoss);
    if (this.narrowFrames <= 0 && this.prng.nextFloat() < collapseChance) {
      this.narrowFrames = 2 + Math.floor(this.prng.nextFloat() * (4 + cfg.bandwidthSwitch * 14));
    } else if (this.narrowFrames > 0) this.narrowFrames--;

    // Repeated 8-40 ms speech grains create the recognizable robot failure.
    const robotChance = cfg.robot * cfg.robot * (0.035 + cfg.packetLoss * 0.28 + cfg.bufferSlip * 0.08);
    if (!this.lossState && this.robotRemain <= 0 && this.prng.nextFloat() < robotChance) {
      const grainMs = 8 + cfg.robot * 24 + this.prng.nextFloat() * 8;
      this.robotLength = Math.max(8, Math.round((grainMs / 1000) * sampleRate));
      this.robotAnchor = this.hi - this.robotLength; this.robotPhase = 0;
      this.robotRemain = this.robotLength * (2 + Math.floor(cfg.robot * 7 + this.prng.nextFloat() * 2));
    }
    this.codecGainTarget = 1 + this.prng.nextSigned() * cfg.codecAmt * 0.045;
    this.frameIndex++;
  }

  process(inputs, outputs, parameters) {
    const out = outputs[0]?.[0];
    if (!out) return true;
    const in0 = inputs[0]?.[0];
    const in1 = inputs[0]?.[1];
    const mode = Math.round(clamp(parameters.mode[0] ?? 0, 0, 3));
    const concealMode = Math.round(clamp(parameters.concealMode[0] ?? 0, 0, 3));
    const packetLoss = clamp01(parameters.packetLoss[0] ?? 0.045);
    const packetMs = clamp(parameters.packetMs[0] ?? 20, 4, 240);
    const repeatMs = clamp(parameters.repeatMs[0] ?? 38, 1, 300);
    const jitterMs = clamp(parameters.jitterMs[0] ?? 0.35, 0, 20);
    const jitterRate = clamp(parameters.jitterRate[0] ?? 18, 1, 220);
    const gate = clamp01(parameters.gate[0] ?? 0.12);
    const bits = Math.round(clamp(parameters.bits[0] ?? 12, 4, 16));
    const rateParam = clamp(parameters.rate[0] ?? 24000, 6000, 48000);
    const robot = clamp01(parameters.robot[0] ?? 0.12);
    const noise = clamp01(parameters.noise[0] ?? 0.12);
    const burstiness = clamp01(parameters.burstiness[0] ?? 0.56);
    const suppression = clamp01(parameters.suppression[0] ?? 0.42);
    const agc = clamp01(parameters.agc[0] ?? 0.34);
    const bufferSlip = clamp01(parameters.bufferSlip[0] ?? 0.08);
    const bandwidthSwitch = clamp01(parameters.bandwidthSwitch[0] ?? 0.12);
    const comfortNoise = clamp01(parameters.comfortNoise[0] ?? 0.22);
    const ceiling = clamp(parameters.ceiling[0] ?? 0.92, 0.2, 1);
    const outGain = clamp(parameters.outGain[0] ?? 0.98, 0, 1.5);

    const modeRateScale = mode === 3 ? 0.72 : mode === 2 ? 0.82 : mode === 1 ? 1 : 0.94;
    const codecAmt = clamp01((14 - bits) / 9 * 0.58 + (32000 - rateParam) / 26000 * 0.58);
    const cfg = { packetLoss, burstiness, jitterMs, jitterRate, bufferSlip, bandwidthSwitch, robot, codecAmt };
    const frameSamps = Math.max(8, Math.round((packetMs / 1000) * sampleRate));
    const envAtk = Math.exp(-1 / (0.0025 * sampleRate));
    const envRel = Math.exp(-1 / ((0.07 + suppression * 0.11) * sampleRate));
    const gateThreshold = 0.0012 + gate * gate * 0.045;
    const suppressFloor = Math.max(0.015, 1 - suppression * 0.96);
    const ratePeriod = Math.max(1, sampleRate / Math.max(6000, Math.min(sampleRate, rateParam * modeRateScale)));
    const quantLevels = Math.pow(2, Math.max(5, bits - (mode === 3 ? 1 : 0)) - 1);
    const profileCutoff = mode === 3 ? 3400 : mode === 2 ? 4400 : mode === 1 ? 7200 : 6000;
    const collapseCutoff = mode === 3 ? 2100 : 2800;
    const comfortBase = noise * (0.00045 + comfortNoise * 0.0028);

    if (this.frameRemain <= 0 || this.frameTotal !== frameSamps) {
      this.frameTotal = frameSamps; this.frameRemain = frameSamps; this._beginFrame(cfg);
    }

    for (let i = 0; i < out.length; i++) {
      if (this.frameRemain <= 0) {
        this.frameRemain = frameSamps; this._beginFrame(cfg);
      }
      const raw = in0 ? (in1 ? 0.5 * (in0[i] + in1[i]) : in0[i]) : 0;
      this.inputDelay[this.di] = raw;
      this.di = (this.di + 1) % this.inputDelay.length;
      this.jitterSmooth += (this.jitterTarget - this.jitterSmooth) * 0.0025;
      let x = this._readCircular(this.inputDelay, this.di, this.jitterSmooth);

      const absx = Math.abs(x);
      const ec = absx > this.env ? envAtk : envRel;
      this.env = ec * this.env + (1 - ec) * absx;
      const speech = this.env > gateThreshold;
      const targetSpeechGain = speech ? 1 : suppressFloor;
      this.speechGain += (targetSpeechGain - this.speechGain) * (speech ? 0.075 : 0.0015 + (1 - suppression) * 0.003);
      x *= this.speechGain;

      const desiredAgc = speech ? clamp(0.115 / Math.max(0.025, this.env), 0.72, 2.6) : 1;
      const agcTarget = 1 + (desiredAgc - 1) * agc;
      this.agcGain += (agcTarget - this.agcGain) * (agcTarget < this.agcGain ? 0.004 : 0.00035);
      x *= this.agcGain;

      const cutoff = this.narrowFrames > 0 ? collapseCutoff : Math.min(profileCutoff, rateParam * 0.44);
      const lpA = Math.exp(-2 * Math.PI * clamp(cutoff, 900, 10000) / sampleRate);
      this.codecLp = (1 - lpA) * x + lpA * this.codecLp;
      const hpA = Math.exp(-2 * Math.PI * (mode === 3 ? 180 : 95) / sampleRate);
      this.codecHpLp = (1 - hpA) * this.codecLp + hpA * this.codecHpLp;
      x = this.codecLp - this.codecHpLp;

      this.ratePhase += 1;
      if (this.ratePhase >= ratePeriod) { this.ratePhase -= ratePeriod; this.rateHold = x; }
      x = x * (1 - codecAmt * 0.38) + this.rateHold * codecAmt * 0.38;
      const mu = 32 + codecAmt * 223;
      const companded = Math.sign(x) * Math.log1p(mu * Math.min(1, Math.abs(x))) / Math.log1p(mu);
      const quantized = Math.round(companded * quantLevels) / quantLevels;
      x = Math.sign(quantized) * Math.expm1(Math.abs(quantized) * Math.log1p(mu)) / mu;
      this.codecGain += (this.codecGainTarget - this.codecGain) * 0.0008;
      x *= this.codecGain;

      const smear = codecAmt * (mode === 2 ? 0.16 : mode === 3 ? 0.12 : 0.09);
      const oldA = this.smearA, oldB = this.smearB;
      this.smearA += (x - this.smearA) * 0.23;
      this.smearB += (oldA - this.smearB) * 0.11;
      x = x * (1 - smear) + (oldA * 0.65 + oldB * 0.35) * smear;

      this.history[this.hi] = x;
      this.hi = (this.hi + 1) % this.historyLen;
      if (this.robotRemain > 0) {
        x = this._historyAt(this.robotAnchor + (this.robotPhase % this.robotLength));
        this.robotPhase++; this.robotRemain--;
      } else if (this.slipRemain > 0) {
        x = this._historyAt(this.slipAnchor + (this.slipPhase % this.slipLength));
        this.slipPhase++; this.slipRemain--;
      }

      if (this.lossState) {
        let concealed = 0;
        if (concealMode !== 1) {
          const plcMs = concealMode === 3 ? repeatMs : concealMode === 0 ? 7 + mode * 1.4 : Math.min(packetMs, 24);
          const plcLen = Math.max(8, Math.min(this.historyLen - 1, Math.round((plcMs / 1000) * sampleRate)));
          concealed = this._historyAt(this.lossAnchor - plcLen + (this.plcPhase % plcLen));
          if (concealMode === 2) concealed *= Math.max(0, 1 - this.plcPhase / Math.max(1, this.frameTotal * 1.8));
          concealed *= Math.pow(concealMode === 3 ? 0.91 : 0.76, Math.max(0, this.lossFrames - 1));
        }
        this.comfort = this.comfort * 0.93 + this.prng.nextSigned() * 0.07;
        concealed += this.comfort * comfortBase * comfortNoise * (concealMode === 1 ? 1 : 0.45);
        x = concealed; this.lastConcealed = concealed; this.plcPhase++;
      } else if (this.recoverRemain > 0) {
        const a = 1 - this.recoverRemain / this.recoverTotal;
        x = this.lastConcealed * (1 - a) + x * a;
        this.recoverRemain--;
      }

      this.comfort = this.comfort * 0.965 + this.prng.nextSigned() * 0.035;
      x += this.comfort * comfortBase * (speech ? 0.12 : 0.35 + suppression * 0.65);
      x = Math.tanh(x * outGain);
      x = clamp(x, -ceiling, ceiling);
      out[i] = Number.isFinite(x) ? x : 0;
      this.frameRemain--;
    }
    return true;
  }
}

registerProcessor("conference", ConferenceProcessor);
