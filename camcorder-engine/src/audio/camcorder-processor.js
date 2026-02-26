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

class CamcorderProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: "coverage", defaultValue: 0.35, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "movement", defaultValue: 0.25, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "corruption", defaultValue: 0.18, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "agcDrive", defaultValue: 0.35, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "wind", defaultValue: 0, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "windLevel", defaultValue: 0.8, minValue: 0, maxValue: 1.5, automationRate: "k-rate" },

      { name: "agcAmt", defaultValue: 0.55, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "agcSpeed", defaultValue: 0.45, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "clip", defaultValue: 0.25, minValue: 0, maxValue: 1, automationRate: "k-rate" },

      { name: "crush", defaultValue: 0.12, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "bits", defaultValue: 12, minValue: 4, maxValue: 16, automationRate: "k-rate" },
      { name: "rate", defaultValue: 24000, minValue: 8000, maxValue: 48000, automationRate: "k-rate" },

      { name: "drop", defaultValue: 0.18, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "dropMs", defaultValue: 28, minValue: 1, maxValue: 500, automationRate: "k-rate" },
      { name: "dropMode", defaultValue: 0, minValue: 0, maxValue: 3, automationRate: "k-rate" }, // 0 hold,1 mute,2 interp,3 repeat
      { name: "repeatMs", defaultValue: 48, minValue: 1, maxValue: 600, automationRate: "k-rate" },
      { name: "chirp", defaultValue: 0.15, minValue: 0, maxValue: 1, automationRate: "k-rate" },

      { name: "handling", defaultValue: 0.22, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "rub", defaultValue: 0.18, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "hiss", defaultValue: 0.12, minValue: 0, maxValue: 1, automationRate: "k-rate" },

      { name: "ceiling", defaultValue: 0.92, minValue: 0.2, maxValue: 1, automationRate: "k-rate" },
      { name: "outGain", defaultValue: 0.98, minValue: 0, maxValue: 1.5, automationRate: "k-rate" },
    ];
  }

  constructor(options) {
    super();
    const seed = (options?.processorOptions?.seed ?? 0x43414d45) >>> 0;
    this.prng = new XorShift32(seed);

    // AGC envelope
    this.env = 0;
    this.limEnv = 0;

    // Dynamic muffling filter state (1-pole lowpass).
    this.mufZ = 0;

    // Sample-rate reduction hold.
    this.hold = 0;
    this.holdCount = 0;
    this.holdPeriod = 1;

    // Concealment/dropouts.
    this.dropBlock = 0;
    this.dropRemain = 0;
    this.dropTotal = 0;
    this.lastGood = 0;
    this.dropStart = 0;
    this.dropEnd = 0;

    // Repeat ring buffer.
    this.ringLen = Math.max(1024, Math.ceil(sampleRate * 0.35));
    this.ring = new Float32Array(this.ringLen);
    this.ri = 0;

    // Handling thump burst.
    this.thumpRemain = 0;
    this.thumpTotal = 0;
    this.thumpPhase = 0;
    this.thumpHz = 55;
    this.thumpAmp = 0;

    // Rub noise bandpass-ish (white -> lowpass@~90Hz subtract -> lowpass@~1.8kHz).
    this.rubLp90 = 0;
    this.rubLp1800 = 0;

    // Chirp burst.
    this.chirpRemain = 0;
    this.chirpTotal = 0;
    this.chirpPhase = 0;
    this.chirpF0 = 1200;
    this.chirpF1 = 6200;
    this.chirpAmp = 0;

    // Wind burst (low freq woof + wideband).
    this.windRemain = 0;
    this.windTotal = 0;
    this.windPhase = 0;
    this.windAmp = 0;

    // Wiggle chunk burst (short clipped crunchy artifacts).
    this.wiggleRemain = 0;
    this.wiggleTotal = 0;
    this.wigglePhase = 0;
    this.wiggleHz = 220;
    this.wiggleAmp = 0;

    // Hiss filter
    this.hissZ = 0;

    this.port.onmessage = (ev) => {
      const msg = ev.data;
      if (msg?.type === "reset") {
        this.prng = new XorShift32((msg.seed ?? 0) >>> 0);
        this.env = 0;
        this.limEnv = 0;
        this.mufZ = 0;
        this.hold = 0;
        this.holdCount = 0;
        this.holdPeriod = 1;
        this.dropBlock = 0;
        this.dropRemain = 0;
        this.dropTotal = 0;
        this.lastGood = 0;
        this.dropStart = 0;
        this.dropEnd = 0;
        this.ring.fill(0);
        this.ri = 0;
        this.thumpRemain = 0;
        this.thumpTotal = 0;
        this.thumpPhase = 0;
        this.thumpHz = 55;
        this.thumpAmp = 0;
        this.rubLp90 = 0;
        this.rubLp1800 = 0;
        this.chirpRemain = 0;
        this.chirpTotal = 0;
        this.chirpPhase = 0;
        this.chirpAmp = 0;
        this.windRemain = 0;
        this.windTotal = 0;
        this.windPhase = 0;
        this.windAmp = 0;
        this.wiggleRemain = 0;
        this.wiggleTotal = 0;
        this.wigglePhase = 0;
        this.wiggleHz = 220;
        this.wiggleAmp = 0;
        this.hissZ = 0;
      }
    };
  }

  process(inputs, outputs, parameters) {
    const output = outputs[0];
    const out = output[0];
    if (!out) return true;
    const in0 = inputs[0]?.[0];
    const in1 = inputs[0]?.[1];

    const coverage = clamp(parameters.coverage[0] ?? 0.35, 0, 1);
    const movement = clamp(parameters.movement[0] ?? 0.25, 0, 1);
    const corruption = clamp(parameters.corruption[0] ?? 0.18, 0, 1);
    const agcDrive = clamp(parameters.agcDrive[0] ?? 0.35, 0, 1);
    const windOn = (parameters.wind[0] ?? 0) >= 0.5;
    const windLevel = clamp(parameters.windLevel[0] ?? 0.8, 0, 1.5);

    const agcAmt = clamp(parameters.agcAmt[0] ?? 0.55, 0, 1);
    const agcSpeed = clamp(parameters.agcSpeed[0] ?? 0.45, 0, 1);
    const clip = clamp(parameters.clip[0] ?? 0.25, 0, 1);

    const crush = clamp(parameters.crush[0] ?? 0.12, 0, 1);
    const bits = Math.round(clamp(parameters.bits[0] ?? 12, 4, 16));
    const rateParam = clamp(parameters.rate[0] ?? 24000, 8000, 48000);

    const drop = clamp(parameters.drop[0] ?? 0.18, 0, 1);
    const dropMs = clamp(parameters.dropMs[0] ?? 28, 1, 600);
    const dropMode = Math.round(clamp(parameters.dropMode[0] ?? 0, 0, 3));
    const repeatMs = clamp(parameters.repeatMs[0] ?? 48, 1, 800);
    const chirp = clamp(parameters.chirp[0] ?? 0.15, 0, 1);

    const handling = clamp(parameters.handling[0] ?? 0.22, 0, 1);
    const rub = clamp(parameters.rub[0] ?? 0.18, 0, 1);
    const hiss = clamp(parameters.hiss[0] ?? 0.12, 0, 1);

    const ceiling = clamp(parameters.ceiling[0] ?? 0.92, 0.2, 1);
    const outGain = clamp(parameters.outGain[0] ?? 0.98, 0, 1.5);

    const sr = sampleRate;

    // AGC tuning.
    const target = 0.18;
    const atk = 0.002 + (1 - agcSpeed) * 0.02;
    const rel = 0.05 + (1 - agcSpeed) * 0.28;
    const envAtk = Math.exp(-1 / (atk * sr));
    const envRel = Math.exp(-1 / (rel * sr));
    const compPow = 1 + agcAmt * 2.0;
    const maxAgc = 7.5;

    // Dynamic muffle cutoff (coverage + loudness closes it).
    const baseCut = 14000 - coverage * 11500; // 14k -> 2.5k
    const loudClose = 0.35 + 0.55 * coverage;

    // ADC crush
    const qLevels = Math.max(1, (1 << (bits - 1)) - 1);
    const rate = Math.min(sr, rateParam);
    const basePeriod = Math.max(1, Math.round(sr / rate));

    // Dropouts
    const dropSamps = Math.max(8, Math.round((dropMs / 1000) * sr));
    const dropP = (0.0000009 + drop * drop * 0.00006) * (1 + 3.2 * corruption);

    // Repeat read offset for conceal.
    const repeatSamps = Math.max(1, Math.round((repeatMs / 1000) * sr));

    // Chirp scheduling
    const chirpP = (0.0000012 + chirp * chirp * 0.00009) * (0.6 + 1.2 * corruption);

    // Handling thumps scheduling
    const thumpP = (0.000004 + movement * movement * 0.00035) * (0.55 + handling);
    const thumpLenMin = Math.max(8, Math.round((10 / 1000) * sr));
    const thumpLenMax = Math.max(thumpLenMin + 1, Math.round((90 / 1000) * sr));

    // Rub noise: band-limited (audible cloth/body handling).
    const rubDepth = rub * rub * (0.055 + 0.095 * movement);
    const rubA90 = Math.exp((-2 * Math.PI * 90) / sr);
    const rubA1800 = Math.exp((-2 * Math.PI * 1800) / sr);

    // Hiss
    const hissDepth = hiss * hiss * 0.03;

    // Drive/clipping
    const drive = 1 + agcDrive * 14;
    const asym = 0.05 * agcDrive;

    // Limiter
    const limAtk = Math.exp(-1 / (0.002 * sr));
    const limRel = Math.exp(-1 / (0.06 * sr));

    // Wind bursts
    const windP = windOn ? (0.0000011 + movement * movement * 0.00022) * 0.9 : 0;
    const windLenMin = Math.max(8, Math.round((35 / 1000) * sr));
    const windLenMax = Math.max(windLenMin + 1, Math.round((180 / 1000) * sr));

    // Wiggle chunk bursts (more likely when movement + corruption/drive are up).
    const wiggleP = (0.0000012 + movement * movement * 0.00018) * (0.35 + 0.9 * corruption);
    const wiggleLenMin = Math.max(8, Math.round((8 / 1000) * sr));
    const wiggleLenMax = Math.max(wiggleLenMin + 1, Math.round((55 / 1000) * sr));

    for (let i = 0; i < out.length; i++) {
      const x = in0 ? (in1 ? 0.5 * (in0[i] + in1[i]) : in0[i]) : 0;
      this.ring[this.ri] = x;
      this.ri = (this.ri + 1) % this.ringLen;

      // Envelope follower on input for AGC + muffling interaction.
      const a0 = Math.abs(x);
      const c0 = a0 > this.env ? envAtk : envRel;
      this.env = a0 + c0 * (this.env - a0);
      const env = this.env + 1e-6;

      // AGC gain.
      const want = Math.pow(target / env, compPow * 0.35);
      const agc = clamp(want, 0.25, maxAgc);
      let y = x * agc;

      // Nonlinear preamp clip.
      if (clip > 0.0001) {
        const amt = 1 + clip * 6.5;
        y = softClip((y + asym) * amt * drive) - asym * 0.7;
      } else {
        y = y * drive;
      }

      // Dynamic muffling: 1-pole lowpass where cutoff closes when loud and covered.
      const loud = clamp(env * 3.2, 0, 1);
      const cut = Math.max(350, baseCut * (1 - loud * loudClose * 0.55));
      const a = Math.exp((-2 * Math.PI * cut) / sr);
      this.mufZ = (1 - a) * y + a * this.mufZ;
      y = this.mufZ;

      // Sample-rate reduction (ZOH) with corruption wobble.
      const wob = corruption * 0.35;
      if (this.holdCount <= 0) {
        const j = wob > 0 ? this.prng.nextSigned() * wob : 0;
        this.holdPeriod = Math.max(1, Math.round(basePeriod * (1 + j)));
        this.hold = y;
        this.holdCount = this.holdPeriod;
      }
      y = this.hold;
      this.holdCount--;

      // Quantization (bitcrush blend).
      if (crush > 0.0001) {
        const q = Math.round(clamp(y, -1, 1) * qLevels) / qLevels;
        y = y * (1 - crush) + q * crush;
      }

      // Start dropout burst?
      if (this.dropRemain <= 0 && this.prng.nextFloat() < dropP) {
        this.dropRemain = dropSamps;
        this.dropTotal = dropSamps;
        this.dropStart = this.lastGood;
        this.dropEnd = x;
      }

      // Apply concealment during dropout.
      if (this.dropRemain > 0) {
        const t = 1 - this.dropRemain / this.dropTotal;
        if (dropMode === 1) y = 0;
        else if (dropMode === 2) y = this.dropStart + (this.dropEnd - this.dropStart) * t;
        else if (dropMode === 3) {
          const read = (this.ri - repeatSamps + this.ringLen) % this.ringLen;
          y = this.ring[read];
        } else y = this.lastGood;
        this.dropRemain--;
      } else {
        this.lastGood = y;
      }

      // Chirp bursts (codec/corruption squeal).
      if (this.chirpRemain <= 0 && chirp > 0.0001 && this.prng.nextFloat() < chirpP) {
        const durMs = 10 + this.prng.nextFloat() * (35 + chirp * 55);
        this.chirpTotal = Math.max(8, Math.round((durMs / 1000) * sr));
        this.chirpRemain = this.chirpTotal;
        this.chirpPhase = this.prng.nextFloat();
        const base = 900 + this.prng.nextFloat() * 2500;
        this.chirpF0 = base;
        this.chirpF1 = base + 3000 + this.prng.nextFloat() * 5000;
        this.chirpAmp = (0.02 + 0.12 * chirp) * (0.65 + 0.7 * this.prng.nextFloat());
      }
      if (this.chirpRemain > 0) {
        const t = 1 - this.chirpRemain / this.chirpTotal;
        const f = this.chirpF0 + (this.chirpF1 - this.chirpF0) * t;
        this.chirpPhase += f / sr;
        if (this.chirpPhase >= 1) this.chirpPhase -= 1;
        const env = Math.sin(Math.PI * t) * (1 - t);
        const sig = Math.sin(this.chirpPhase * 2 * Math.PI) * this.chirpAmp * env;
        y = clamp(y + sig, -1.2, 1.2);
        this.chirpRemain--;
      }

      // Handling thumps (low freq bursts).
      if (this.thumpRemain <= 0 && handling > 0.0001 && this.prng.nextFloat() < thumpP) {
        const len = thumpLenMin + Math.floor(this.prng.nextFloat() * (thumpLenMax - thumpLenMin));
        this.thumpTotal = len;
        this.thumpRemain = len;
        this.thumpPhase = 0;
        this.thumpHz = 35 + this.prng.nextFloat() * 75;
        this.thumpAmp = (0.045 + 0.38 * handling) * (0.75 + 0.9 * this.prng.nextFloat());
      }
      if (this.thumpRemain > 0) {
        const t = 1 - this.thumpRemain / this.thumpTotal;
        const env = Math.pow(1 - t, 2.05) * Math.sin(Math.min(1, t / 0.1) * Math.PI * 0.5);
        this.thumpPhase += this.thumpHz / sr;
        if (this.thumpPhase >= 1) this.thumpPhase -= 1;
        const sig = Math.sin(this.thumpPhase * 2 * Math.PI) * this.thumpAmp * env;
        y = y + sig;
        this.thumpRemain--;
      }

      // Wiggle chunk (short crunchy clipped bursts).
      if (this.wiggleRemain <= 0 && movement > 0.0001 && this.prng.nextFloat() < wiggleP) {
        const len = wiggleLenMin + Math.floor(this.prng.nextFloat() * (wiggleLenMax - wiggleLenMin));
        this.wiggleTotal = len;
        this.wiggleRemain = len;
        this.wigglePhase = this.prng.nextFloat();
        this.wiggleHz = 140 + this.prng.nextFloat() * 520;
        this.wiggleAmp = (0.02 + 0.16 * movement) * (0.6 + 0.8 * this.prng.nextFloat());
      }
      if (this.wiggleRemain > 0) {
        const t = 1 - this.wiggleRemain / this.wiggleTotal;
        const env = Math.pow(1 - t, 1.8);
        this.wigglePhase += this.wiggleHz / sr;
        if (this.wigglePhase >= 1) this.wigglePhase -= 1;
        const p = this.wigglePhase < 0.5 ? 1 : -1;
        const chunk = softClip(p * this.wiggleAmp * (1 + 2.2 * corruption));
        y = y + chunk * env;
        this.wiggleRemain--;
      }

      // Wind/cloth hits (woof + wideband, triggers the muffling/AGC naturally).
      if (windOn && this.windRemain <= 0 && this.prng.nextFloat() < windP) {
        const len = windLenMin + Math.floor(this.prng.nextFloat() * (windLenMax - windLenMin));
        this.windTotal = len;
        this.windRemain = len;
        this.windPhase = this.prng.nextFloat();
        this.windAmp = (0.09 + 0.7 * movement) * (0.75 + 0.85 * this.prng.nextFloat());
      }
      if (this.windRemain > 0) {
        const t = 1 - this.windRemain / this.windTotal;
        const env = Math.sin(Math.PI * t) * (1 - 0.2 * t);
        const f = 35 + 55 * (1 - t);
        this.windPhase += f / sr;
        if (this.windPhase >= 1) this.windPhase -= 1;
        const woof = Math.sin(this.windPhase * 2 * Math.PI) * this.windAmp;
        const wide = this.prng.nextSigned() * (this.windAmp * 0.55);
        const wl = windLevel;
        y = y + (woof + wide) * env * wl;
        this.windRemain--;
      }

      // Rub noise (cloth/body).
      if (rubDepth > 0.00001) {
        const wn = this.prng.nextSigned();
        // lowpass@90
        this.rubLp90 = (1 - rubA90) * wn + rubA90 * this.rubLp90;
        const hp = wn - this.rubLp90;
        // lowpass@1800
        this.rubLp1800 = (1 - rubA1800) * hp + rubA1800 * this.rubLp1800;
        y = y + this.rubLp1800 * rubDepth;
      }

      // Hiss (HP-ish).
      if (hissDepth > 0.00001) {
        const wn = this.prng.nextSigned();
        const hp = wn - this.hissZ;
        this.hissZ = wn;
        y = y + hp * hissDepth;
      }

      // Final limiter.
      let post = y * outGain;
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

registerProcessor("camcorder", CamcorderProcessor);
