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
    return (this.nextU32() >>> 8) / 16777216;
  }
}

function clamp01(x) {
  return Math.min(1, Math.max(0, x));
}

class TvNoiseProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: "level", defaultValue: 0.12, minValue: 0, maxValue: 1, automationRate: "a-rate" },
      { name: "hiss", defaultValue: 0.55, minValue: 0, maxValue: 1, automationRate: "a-rate" },
      { name: "crackle", defaultValue: 0.08, minValue: 0, maxValue: 1, automationRate: "a-rate" },
      { name: "seed", defaultValue: 1, minValue: 0, maxValue: 4294967295, automationRate: "k-rate" },
    ];
  }

  constructor(options) {
    super();
    const seed = (options?.processorOptions?.seed ?? 0xdecafbad) >>> 0;
    this.prng = new XorShift32(seed);
    this.hp = 0;
    this.lp = 0;
    this.hpA = 0.6;
    this.lpA = 0.03;
    this.crackleHold = 0;
  }

  process(inputs, outputs, parameters) {
    const out = outputs[0];
    if (!out || !out[0]) return true;
    const ch0 = out[0];

    const level = parameters.level;
    const hiss = parameters.hiss;
    const crackle = parameters.crackle;
    const seed = parameters.seed;
    if (seed && seed.length) {
      const s = (seed[0] >>> 0) || 1;
      if ((this._seedLast ?? 0) !== s) {
        this._seedLast = s;
        this.prng = new XorShift32(s);
      }
    }

    for (let i = 0; i < ch0.length; i++) {
      const lvl = clamp01(level.length > 1 ? level[i] : level[0]);
      const h = clamp01(hiss.length > 1 ? hiss[i] : hiss[0]);
      const c = clamp01(crackle.length > 1 ? crackle[i] : crackle[0]);

      let x = this.prng.nextFloat() * 2 - 1; // white

      // highpass-ish (emphasize fizz)
      this.hp = this.hpA * (this.hp + x - (this._xPrev ?? 0));
      this._xPrev = x;

      // lowpass-ish (remove harshness as hiss goes down)
      const lpA = 0.004 + (1 - h) * 0.06;
      this.lpA = this.lpA * 0.98 + lpA * 0.02;
      this.lp = this.lp + (this.hp - this.lp) * this.lpA;

      // crackle: sparse short impulses
      if (this.crackleHold > 0) {
        this.crackleHold--;
      } else {
        const p = 0.00001 + c * 0.00028;
        if (this.prng.nextFloat() < p) {
          this.crackleHold = 8 + ((this.prng.nextU32() >>> 0) % 24);
          this._crackleAmp = 0.7 + this.prng.nextFloat() * 0.6;
        }
      }
      const crack = this.crackleHold > 0 ? (this._crackleAmp ?? 1) * (this.prng.nextFloat() * 2 - 1) : 0;

      ch0[i] = (this.lp + crack * 0.25) * lvl;
    }
    return true;
  }
}

registerProcessor("tv-noise", TvNoiseProcessor);

