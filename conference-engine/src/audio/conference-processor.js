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
function clamp01(x) {
  return Math.min(1, Math.max(0, x));
}
function softClip(x) {
  return Math.tanh(x);
}

class ConferenceProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: "mode", defaultValue: 0, minValue: 0, maxValue: 3, automationRate: "k-rate" }, // 0 discord,1 zoom,2 skype,3 cell
      { name: "concealMode", defaultValue: 0, minValue: 0, maxValue: 3, automationRate: "k-rate" }, // 0 hold,1 mute,2 interp,3 repeat
      { name: "packetLoss", defaultValue: 0.18, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "packetMs", defaultValue: 24, minValue: 4, maxValue: 240, automationRate: "k-rate" },
      { name: "repeatMs", defaultValue: 42, minValue: 1, maxValue: 300, automationRate: "k-rate" },
      { name: "jitterMs", defaultValue: 0.12, minValue: 0, maxValue: 3, automationRate: "k-rate" },
      { name: "jitterRate", defaultValue: 34, minValue: 1, maxValue: 220, automationRate: "k-rate" },
      { name: "gate", defaultValue: 0.12, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "bits", defaultValue: 12, minValue: 4, maxValue: 16, automationRate: "k-rate" },
      { name: "rate", defaultValue: 24000, minValue: 6000, maxValue: 48000, automationRate: "k-rate" },
      { name: "robot", defaultValue: 0.12, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "noise", defaultValue: 0.12, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "ceiling", defaultValue: 0.92, minValue: 0.2, maxValue: 1, automationRate: "k-rate" },
      { name: "outGain", defaultValue: 0.98, minValue: 0, maxValue: 1.5, automationRate: "k-rate" },
    ];
  }

  constructor(options) {
    super();
    const seed = (options?.processorOptions?.seed ?? 0x636f6e66) >>> 0;
    this.prng = new XorShift32(seed);

    this.delayLen = Math.max(256, Math.ceil(sampleRate * 0.012));
    this.delay = new Float32Array(this.delayLen);
    this.di = 0;
    this.jPhase = this.prng.nextFloat();
    this.jNoise = 0;

    this.ringLen = Math.max(1024, Math.ceil(sampleRate * 0.4));
    this.ring = new Float32Array(this.ringLen);
    this.ri = 0;

    this.env = 0;
    this.gateGain = 1;

    this.packetRemain = 0;
    this.packetTotal = 0;
    this.inDrop = false;
    this.dropFade = 0;
    this.lastGood = 0;
    this.dropStart = 0;

    this.hold = 0;
    this.holdPeriod = 1;
    this.rateAcc = 0;

    this.robotRemain = 0;
    this.robotLen = 0;
    this.robotI = 0;
    this.robotBuf = new Float32Array(Math.max(64, Math.ceil(sampleRate * 0.09)));

    this.port.onmessage = (ev) => {
      const msg = ev.data;
      if (msg?.type === "reset") {
        this.prng = new XorShift32((msg.seed ?? 0) >>> 0);
        this.delay.fill(0);
        this.di = 0;
        this.jPhase = this.prng.nextFloat();
        this.jNoise = 0;
        this.ring.fill(0);
        this.ri = 0;
        this.env = 0;
        this.gateGain = 1;
        this.packetRemain = 0;
        this.packetTotal = 0;
        this.inDrop = false;
        this.dropFade = 0;
        this.lastGood = 0;
        this.dropStart = 0;
        this.hold = 0;
        this.holdPeriod = 1;
        this.rateAcc = 0;
        this.robotRemain = 0;
        this.robotLen = 0;
        this.robotI = 0;
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

  _startPacket(packetSamps, lossProb) {
    this.packetTotal = Math.max(1, packetSamps | 0);
    this.packetRemain = this.packetTotal;
    const wasDrop = this.inDrop;
    this.inDrop = this.prng.nextFloat() < lossProb;
    if (this.inDrop) {
      this.dropStart = this.lastGood;
    } else if (wasDrop) {
      this.dropFade = 0.12;
    }
  }

  process(inputs, outputs, parameters) {
    const out = outputs[0]?.[0];
    if (!out) return true;
    const in0 = inputs[0]?.[0];
    const in1 = inputs[0]?.[1];

    const mode = Math.round(clamp(parameters.mode[0] ?? 0, 0, 3));
    const concealMode = Math.round(clamp(parameters.concealMode[0] ?? 0, 0, 3));
    const packetLoss = clamp01(parameters.packetLoss[0] ?? 0.18);
    const packetMs = clamp(parameters.packetMs[0] ?? 24, 4, 240);
    const repeatMs = clamp(parameters.repeatMs[0] ?? 42, 1, 300);
    const jitterMs = clamp(parameters.jitterMs[0] ?? 0.12, 0, 3);
    const jitterRate = clamp(parameters.jitterRate[0] ?? 34, 1, 220);
    const gate = clamp01(parameters.gate[0] ?? 0.12);
    const bits = Math.round(clamp(parameters.bits[0] ?? 12, 4, 16));
    const rateParam = clamp(parameters.rate[0] ?? 24000, 6000, 48000);
    const robot = clamp01(parameters.robot[0] ?? 0.12);
    const noise = clamp01(parameters.noise[0] ?? 0.12);
    const ceiling = clamp(parameters.ceiling[0] ?? 0.92, 0.2, 1);
    const outGain = clamp(parameters.outGain[0] ?? 0.98, 0, 1.5);

    const sr = sampleRate;
    const modeLossScale = mode === 3 ? 1.35 : mode === 2 ? 1.05 : mode === 1 ? 0.95 : 1.0;
    const lossProb = clamp01(packetLoss * modeLossScale);

    const packetSamps = Math.max(1, Math.round((packetMs / 1000) * sr));
    if (this.packetRemain <= 0 || this.packetTotal !== packetSamps) this._startPacket(packetSamps, lossProb);

    const jDepth = (jitterMs / 1000) * sr;
    const jInc = (2 * Math.PI * jitterRate) / sr;

    this.holdPeriod = Math.max(1, Math.round(sr / Math.max(6000, Math.min(sr, rateParam))));
    const q = Math.pow(2, bits - 1);

    const envAtk = Math.exp(-1 / (0.003 * sr));
    const envRel = Math.exp(-1 / (0.06 * sr));
    const gateThr = 0.002 + gate * gate * 0.06;

    const robotRate = 0.05 + robot * 1.25; // events/sec
    const robotP = robotRate / sr;

    for (let i = 0; i < out.length; i++) {
      const xIn = in0 ? (in1 ? 0.5 * (in0[i] + in1[i]) : in0[i]) : 0;

      this.delay[this.di] = xIn;
      this.di = (this.di + 1) % this.delayLen;

      this.jNoise = 0.995 * this.jNoise + 0.005 * this.prng.nextSigned();
      const jMod = 0.65 + 0.35 * Math.sin(this.jPhase);
      this.jPhase += jInc;
      if (this.jPhase > Math.PI * 2) this.jPhase -= Math.PI * 2;
      const jSamps = jDepth * (jMod + 0.25 * this.jNoise);
      let x = this._readDelay(Math.max(0, jSamps));

      const absx = Math.abs(x);
      const e = absx > this.env ? envAtk : envRel;
      this.env = (1 - e) * absx + e * this.env;
      const wantGate = this.env < gateThr ? 0.1 : 1;
      this.gateGain = 0.995 * this.gateGain + 0.005 * wantGate;
      x *= this.gateGain;

      if (robot > 0.0001 && this.robotRemain <= 0 && this.prng.nextFloat() < robotP) {
        const durMs = 18 + robot * 90;
        this.robotLen = Math.max(8, Math.min(this.robotBuf.length, Math.round((durMs / 1000) * sr)));
        this.robotI = 0;
        this.robotRemain = Math.max(8, Math.round(this.robotLen * (1.2 + robot * 3.2)));
        for (let k = 0; k < this.robotLen; k++) {
          const idx = (this.di - 1 - k + this.delayLen) % this.delayLen;
          this.robotBuf[this.robotLen - 1 - k] = this.delay[idx];
        }
      }
      if (this.robotRemain > 0 && this.robotLen > 0) {
        x = this.robotBuf[this.robotI];
        this.robotI = (this.robotI + 1) % this.robotLen;
        this.robotRemain--;
      }

      if (this.inDrop) {
        if (concealMode === 1) x = 0;
        else if (concealMode === 3) {
          const back = Math.max(1, Math.round((repeatMs / 1000) * sr));
          const idx = (this.ri - back + this.ringLen) % this.ringLen;
          x = this.ring[idx];
        } else {
          x = this.lastGood;
        }
      } else {
        this.lastGood = x;
      }

      this.packetRemain--;
      if (this.packetRemain <= 0) this._startPacket(packetSamps, lossProb);

      if (!this.inDrop && this.dropFade > 0) {
        const a = clamp01(this.dropFade);
        x = this.dropStart * (1 - a) + x * a;
        this.dropFade += 0.08;
        if (this.dropFade >= 1) this.dropFade = 0;
      }

      this.rateAcc += 1;
      if (this.rateAcc >= this.holdPeriod) {
        this.rateAcc = 0;
        this.hold = x;
      }
      x = this.hold;

      x = Math.round(x * q) / q;

      const hiss = noise * (0.002 + (mode === 3 ? 0.002 : 0.001));
      x += this.prng.nextSigned() * hiss;

      x *= outGain;
      x = softClip(x);
      x = clamp(x, -ceiling, ceiling);
      out[i] = x;

      this.ring[this.ri] = out[i];
      this.ri = (this.ri + 1) % this.ringLen;
    }

    return true;
  }
}

registerProcessor("conference", ConferenceProcessor);

