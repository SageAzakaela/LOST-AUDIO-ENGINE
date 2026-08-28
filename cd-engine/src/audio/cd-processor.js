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

function crossedPhase(previous, next, target) {
  return next >= previous
    ? target > previous && target <= next
    : target > previous || target <= next;
}

class CdProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: "mode", defaultValue: 2, minValue: 0, maxValue: 4, automationRate: "k-rate" }, // 0 hold, 1 mute, 2 interpolate, 3 repeat, 4 random per event
      { name: "damageShape", defaultValue: 0, minValue: 0, maxValue: 5, automationRate: "k-rate" }, // radial, sine, triangle, square, saw, random pits
      { name: "errorRate", defaultValue: 0.12, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "burstMs", defaultValue: 18, minValue: 1, maxValue: 600, automationRate: "k-rate" },
      { name: "repeatMs", defaultValue: 36, minValue: 1, maxValue: 400, automationRate: "k-rate" },
      { name: "scratchRate", defaultValue: 0.14, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "scratchAmt", defaultValue: 0.2, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "correction", defaultValue: 0.88, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "interpolationMs", defaultValue: 5, minValue: 0.25, maxValue: 30, automationRate: "k-rate" },
      { name: "rotationHz", defaultValue: 5.2, minValue: 2, maxValue: 10, automationRate: "k-rate" },
      { name: "trackingRate", defaultValue: 0.08, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "trackingMs", defaultValue: 140, minValue: 10, maxValue: 1800, automationRate: "k-rate" },
      { name: "servoHunt", defaultValue: 0.18, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "jitterMs", defaultValue: 0.025, minValue: 0, maxValue: 2, automationRate: "k-rate" },
      { name: "jitterRate", defaultValue: 34, minValue: 1, maxValue: 200, automationRate: "k-rate" },
      { name: "hfLoss", defaultValue: 0.025, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "servoNoise", defaultValue: 0.08, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "softClip", defaultValue: 1, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "ceiling", defaultValue: 0.94, minValue: 0.2, maxValue: 1, automationRate: "k-rate" },
      { name: "outGain", defaultValue: 0.98, minValue: 0, maxValue: 1.2, automationRate: "k-rate" },
    ];
  }

  constructor(options) {
    super();
    const seed = (options?.processorOptions?.seed ?? 0x4344454e) >>> 0;
    this._reset(seed);

    this.port.onmessage = (event) => {
      if (event.data?.type === "reset") this._reset((event.data.seed ?? 0) >>> 0);
      if (event.data?.type === "triggerDamage") {
        this.pendingDamage = Math.max(this.pendingDamage, clamp(event.data.strength ?? 1, 0.05, 1));
      }
      if (event.data?.type === "triggerSkip") {
        this.pendingSkip = Math.max(this.pendingSkip, clamp(event.data.strength ?? 1, 0.05, 1));
      }
    };
  }

  _reset(seed) {
    this.prng = new XorShift32(seed);

    // Real CD timing errors are normally corrected; the upper range is an
    // intentionally exaggerated modulation effect.
    this.delayLen = Math.max(256, Math.ceil(sampleRate * 0.006));
    this.delay = new Float32Array(this.delayLen);
    this.di = 0;
    this.jPhase = this.prng.nextFloat();
    this.jNoise = 0;

    // A two-second history lets a lost optical read head jump backward and loop
    // actual decoded audio instead of producing a generic delay tap.
    this.ringLen = Math.max(4096, Math.ceil(sampleRate * 2.1));
    this.ring = new Float32Array(this.ringLen);
    this.ri = 0;
    this.ringFilled = 0;
    this.repeatStart = 0;
    this.repeatPos = 0;
    this.repeatLength = 1;
    this.trackingRemain = 0;

    this.errRemain = 0;
    this.errTotal = 0;
    this.errStage = 0; // 0 corrected, 1 interpolated, 2 terminal concealment
    this.activeErrMode = 2;
    this.pendingDamage = 0;
    this.pendingSkip = 0;
    this.lastGood = 0;
    this.lastGoodDelta = 0;

    // A scratch recurs once per revolution. Two fixed angular marks make that
    // recurrence audible without injecting unrelated broadband clicks.
    this.discPhase = this.prng.nextFloat();
    this.scratchPhaseA = this.prng.nextFloat();
    this.scratchPhaseB = (this.scratchPhaseA + 0.17 + this.prng.nextFloat() * 0.46) % 1;
    this.damageBucket = -1;
    this.damageRandomValue = this.prng.nextFloat();

    this.servoEnv = 0;
    this.servoSweep = 0;
    this.servoPhase = this.prng.nextFloat();
    this.servoPhase2 = this.prng.nextFloat();
    this.hfZ = 0;
    this.limEnv = 0;
  }

  _readDelay(delaySamples) {
    const read = this.di - delaySamples;
    const base = Math.floor(read);
    const r0 = ((base % this.delayLen) + this.delayLen) % this.delayLen;
    const r1 = (r0 + 1) % this.delayLen;
    const frac = read - base;
    return this.delay[r0] * (1 - frac) + this.delay[r1] * frac;
  }

  _beginRepeat(offsetSamples, loopSamples, durationSamples) {
    const available = Math.max(2, Math.min(this.ringLen - 2, this.ringFilled));
    const safeOffset = Math.min(available, Math.max(2, offsetSamples));
    this.repeatStart = (this.ri - safeOffset + this.ringLen) % this.ringLen;
    this.repeatPos = 0;
    this.repeatLength = Math.max(1, Math.min(loopSamples, safeOffset - 1));
    this.trackingRemain = Math.max(this.repeatLength, durationSamples);
  }

  _resolveMode(mode) {
    if (mode !== 4) return mode;
    // Resolve once per failed read so Random behaves like a changing recovery
    // strategy, never as unsafe sample-by-sample switching.
    const pick = this.prng.nextFloat();
    if (pick < 0.22) return 0;
    if (pick < 0.36) return 1;
    if (pick < 0.62) return 2;
    return 3;
  }

  _damageWave(shape, phase) {
    if (shape === 1) return 0.5 + Math.sin(phase * Math.PI * 2) * 0.5;
    if (shape === 2) return 1 - Math.abs(phase * 2 - 1);
    if (shape === 3) return phase < 0.3 ? 1 : 0.035;
    if (shape === 4) return phase;
    if (shape === 5) {
      const bucket = Math.floor(phase * 12);
      if (bucket !== this.damageBucket) {
        this.damageBucket = bucket;
        this.damageRandomValue = this.prng.nextFloat();
      }
      return this.damageRandomValue;
    }
    return 0;
  }

  process(inputs, outputs, parameters) {
    const out = outputs[0]?.[0];
    if (!out) return true;
    const in0 = inputs[0]?.[0];
    const in1 = inputs[0]?.[1];

    const mode = Math.round(clamp(parameters.mode[0] ?? 2, 0, 4));
    const damageShape = Math.round(clamp(parameters.damageShape[0] ?? 0, 0, 5));
    const errorRate = clamp(parameters.errorRate[0] ?? 0.12, 0, 1);
    const burstMs = clamp(parameters.burstMs[0] ?? 18, 1, 600);
    const repeatMs = clamp(parameters.repeatMs[0] ?? 36, 1, 400);
    const scratchRate = clamp(parameters.scratchRate[0] ?? 0.14, 0, 1);
    const scratchAmt = clamp(parameters.scratchAmt[0] ?? 0.2, 0, 1);
    const correction = clamp(parameters.correction[0] ?? 0.88, 0, 1);
    const interpolationMs = clamp(parameters.interpolationMs[0] ?? 5, 0.25, 30);
    const rotationHz = clamp(parameters.rotationHz[0] ?? 5.2, 2, 10);
    const trackingRate = clamp(parameters.trackingRate[0] ?? 0.08, 0, 1);
    const trackingMs = clamp(parameters.trackingMs[0] ?? 140, 10, 1800);
    const servoHunt = clamp(parameters.servoHunt[0] ?? 0.18, 0, 1);
    const jitterMs = clamp(parameters.jitterMs[0] ?? 0.025, 0, 2);
    const jitterRate = clamp(parameters.jitterRate[0] ?? 34, 1, 200);
    const hfLoss = clamp(parameters.hfLoss[0] ?? 0.025, 0, 1);
    const servoNoise = clamp(parameters.servoNoise[0] ?? 0.08, 0, 1);
    const doClip = (parameters.softClip[0] ?? 1) >= 0.5;
    const ceiling = clamp(parameters.ceiling[0] ?? 0.94, 0.2, 1);
    const outGain = clamp(parameters.outGain[0] ?? 0.98, 0, 1.2);

    const sr = sampleRate;
    const burstSamples = Math.max(1, Math.round((burstMs / 1000) * sr));
    const interpolationSamples = Math.max(1, Math.round((interpolationMs / 1000) * sr));
    const repeatSamples = Math.max(1, Math.round((repeatMs / 1000) * sr));
    const trackingOffset = Math.max(repeatSamples + 1, Math.round((trackingMs / 1000) * sr));

    const jitterDepth = (Math.min(1.25, jitterMs) / 1000) * sr;
    const jitterHz = 7 + jitterRate;
    const randomErrorP = errorRate * errorRate * 0.000025;
    const trackingP = trackingRate * trackingRate * (0.000004 + (1 - correction) * 0.000016);
    const servoDecay = Math.exp(-1 / ((0.055 + servoHunt * 0.3) * sr));

    const hfCut = 3200 + (1 - hfLoss) * 17400;
    const hfA = Math.exp((-2 * Math.PI * hfCut) / sr);
    const limAtk = Math.exp(-1 / (0.0015 * sr));
    const limRel = Math.exp(-1 / (0.065 * sr));

    for (let i = 0; i < out.length; i++) {
      const input = in0 ? (in1 ? 0.5 * (in0[i] + in1[i]) : in0[i]) : 0;

      this.delay[this.di] = input;
      this.jNoise = this.jNoise * 0.997 + this.prng.nextSigned() * 0.003;
      this.jPhase = (this.jPhase + jitterHz / sr) % 1;
      const jitterMod = clamp(Math.sin(this.jPhase * Math.PI * 2) * 0.72 + this.jNoise * 0.8, -1, 1);
      const delaySamples = jitterDepth * (0.5 + jitterMod * 0.5);
      let y = this._readDelay(delaySamples);
      this.di = (this.di + 1) % this.delayLen;

      this.ring[this.ri] = y;
      this.ri = (this.ri + 1) % this.ringLen;
      this.ringFilled = Math.min(this.ringLen, this.ringFilled + 1);

      const oldDiscPhase = this.discPhase;
      this.discPhase = (this.discPhase + rotationHz / sr) % 1;
      const passedA = crossedPhase(oldDiscPhase, this.discPhase, this.scratchPhaseA);
      const passedB = crossedPhase(oldDiscPhase, this.discPhase, this.scratchPhaseB);
      const shapedDamage = this._damageWave(damageShape, this.discPhase);
      let defectSeverity = 0;
      const manualDamage = this.pendingDamage > 0 && this.errRemain <= 0 && this.trackingRemain <= 0;

      if (manualDamage) {
        defectSeverity = this.pendingDamage;
        this.pendingDamage = 0;
      }

      if (this.errRemain <= 0 && this.trackingRemain <= 0 && this.prng.nextFloat() < randomErrorP) {
        defectSeverity = (0.16 + errorRate * 0.72) * (0.55 + this.prng.nextFloat() * 0.45);
      }
      if (damageShape === 0 && this.errRemain <= 0 && this.trackingRemain <= 0 && (passedA || passedB)) {
        const markScale = passedA ? 1 : 0.68;
        if (this.prng.nextFloat() < scratchRate * markScale) {
          defectSeverity = Math.max(defectSeverity, scratchAmt * markScale * (0.72 + this.prng.nextFloat() * 0.28));
        }
      }
      if (damageShape !== 0 && this.errRemain <= 0 && this.trackingRemain <= 0) {
        const shapedProbability = scratchRate * scratchRate * 0.00004 * (0.025 + shapedDamage * shapedDamage * 1.7);
        if (this.prng.nextFloat() < shapedProbability) {
          defectSeverity = Math.max(
            defectSeverity,
            scratchAmt * (0.42 + shapedDamage * 0.58) * (0.72 + this.prng.nextFloat() * 0.28),
          );
        }
      }

      if (defectSeverity > 0) {
        const wasManual = manualDamage;
        const recoveryDemand = defectSeverity * (0.78 + this.prng.nextFloat() * 0.32);
        this.servoEnv = Math.max(this.servoEnv, defectSeverity * (0.22 + servoHunt * 0.78));
        this.servoSweep = Math.max(this.servoSweep, defectSeverity);

        if (wasManual || recoveryDemand > correction) {
          const overload = clamp((recoveryDemand - correction) / Math.max(0.08, 1 - correction), 0, 1);
          const length = Math.max(1, Math.round(burstSamples * (wasManual ? 1.35 : 0.25 + overload * 1.4)));
          this.errRemain = length;
          this.errTotal = length;
          this.activeErrMode = this._resolveMode(mode);
          this.errStage = mode === 4
            ? (this.activeErrMode === 2 ? 1 : 2)
            : (length <= interpolationSamples || overload < 0.32 ? 1 : 2);

          if (this.errStage === 2 && this.activeErrMode === 3) {
            const available = Math.max(2, Math.min(this.ringLen - 2, this.ringFilled));
            const safeOffset = Math.min(available, Math.max(2, repeatSamples));
            this.repeatStart = (this.ri - safeOffset + this.ringLen) % this.ringLen;
            this.repeatPos = 0;
            this.repeatLength = Math.max(1, Math.min(repeatSamples, safeOffset - 1));
          }
        }
      }

      const skipHistoryNeeded = Math.max(repeatSamples * 2, Math.min(trackingOffset, Math.round(sr * 0.08)));
      if (this.pendingSkip > 0 && this.trackingRemain <= 0 && this.ringFilled >= skipHistoryNeeded) {
        const strength = this.pendingSkip;
        this.pendingSkip = 0;
        const loopDuration = Math.round(sr * (0.22 + strength * 1.45));
        this._beginRepeat(trackingOffset, repeatSamples, loopDuration);
        this.servoEnv = Math.max(this.servoEnv, 0.45 + strength * 0.55);
        this.servoSweep = Math.max(this.servoSweep, 0.55 + strength * 0.45);
        this.errRemain = 0;
      }

      if (this.trackingRemain <= 0 && this.prng.nextFloat() < trackingP) {
        const loopDuration = Math.round(sr * (0.16 + trackingRate * 1.75));
        this._beginRepeat(trackingOffset, repeatSamples, loopDuration);
        this.servoEnv = Math.max(this.servoEnv, 0.35 + trackingRate * 0.65);
        this.servoSweep = Math.max(this.servoSweep, 0.5 + servoHunt * 0.5);
        this.errRemain = 0;
      }

      if (this.trackingRemain > 0) {
        const ringIndex = (this.repeatStart + (this.repeatPos % this.repeatLength)) % this.ringLen;
        y = this.ring[ringIndex];
        this.repeatPos++;
        this.trackingRemain--;
      } else if (this.errRemain > 0) {
        const progress = 1 - this.errRemain / Math.max(1, this.errTotal);
        if (this.errStage === 1) {
          const predicted = this.lastGood + this.lastGoodDelta * Math.min(20, this.errTotal - this.errRemain + 1);
          const returnBlend = 0.12 + progress * progress * 0.82;
          y = predicted * (1 - returnBlend) + y * returnBlend;
        } else if (this.activeErrMode === 1) {
          const edge = Math.min(1, progress * 12, (this.errRemain / Math.max(1, this.errTotal)) * 12);
          y *= 1 - edge;
        } else if (this.activeErrMode === 2) {
          const predicted = this.lastGood + this.lastGoodDelta * Math.min(12, this.errTotal - this.errRemain + 1);
          y = predicted * (1 - progress * 0.65) + y * progress * 0.65;
        } else if (this.activeErrMode === 3) {
          const ringIndex = (this.repeatStart + (this.repeatPos % this.repeatLength)) % this.ringLen;
          y = this.ring[ringIndex];
          this.repeatPos++;
        } else {
          y = this.lastGood;
        }
        this.errRemain--;
      } else {
        const delta = y - this.lastGood;
        this.lastGoodDelta = this.lastGoodDelta * 0.82 + delta * 0.18;
        this.lastGood = y;
      }

      this.servoEnv *= servoDecay;
      this.servoSweep *= 0.9996;
      if (servoNoise > 0.0001 && this.servoEnv > 0.00005) {
        const seek = servoHunt * this.servoSweep;
        const motorHz = 82 + rotationHz * 10 + seek * 170;
        const focusHz = 540 + seek * 1250;
        this.servoPhase = (this.servoPhase + motorHz / sr) % 1;
        this.servoPhase2 = (this.servoPhase2 + focusHz / sr) % 1;
        const motor = Math.sin(this.servoPhase * Math.PI * 2);
        const focus = Math.sin(this.servoPhase2 * Math.PI * 2 + Math.sin(this.servoPhase * Math.PI * 2) * 0.7);
        y += (motor * 0.62 + focus * 0.38) * this.servoEnv * servoNoise * 0.018;
      }

      this.hfZ = (1 - hfA) * y + hfA * this.hfZ;
      y = this.hfZ;

      let post = y * outGain;
      if (doClip) post = Math.tanh(post);

      const amplitude = Math.abs(post);
      const coefficient = amplitude > this.limEnv ? limAtk : limRel;
      this.limEnv = amplitude + coefficient * (this.limEnv - amplitude);
      if (this.limEnv > ceiling) post *= ceiling / (this.limEnv + 1e-6);
      out[i] = clamp(post, -ceiling, ceiling);
    }

    return true;
  }
}

registerProcessor("cd", CdProcessor);
