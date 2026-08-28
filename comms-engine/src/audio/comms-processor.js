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

class CommsProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: "drive", defaultValue: 0.35, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "comp", defaultValue: 0.45, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "bits", defaultValue: 12, minValue: 4, maxValue: 16, automationRate: "k-rate" },
      { name: "rate", defaultValue: 24000, minValue: 6000, maxValue: 48000, automationRate: "k-rate" },
      { name: "packet", defaultValue: 0.2, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "packetMs", defaultValue: 28, minValue: 8, maxValue: 160, automationRate: "k-rate" },
      { name: "hum", defaultValue: 0.25, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "hiss", defaultValue: 0.22, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "toneMix", defaultValue: 0.35, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "transducer", defaultValue: 0.45, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "lineAge", defaultValue: 0.2, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "duplex", defaultValue: 0.08, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "speakerRattle", defaultValue: 0.12, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "alarm", defaultValue: 0, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "mode", defaultValue: 0, minValue: 0, maxValue: 4, automationRate: "k-rate" }, // 0 landline,1 cell,2 intercom,3 pa,4 alarm
      { name: "ceiling", defaultValue: 0.92, minValue: 0.2, maxValue: 1, automationRate: "k-rate" },
      { name: "outGain", defaultValue: 0.95, minValue: 0, maxValue: 1.5, automationRate: "k-rate" },
    ];
  }

  constructor(options) {
    super();
    const seed = (options?.processorOptions?.seed ?? 0x6f6d6d73) >>> 0;
    this.prng = new XorShift32(seed);

    this.env = 0;
    this.limEnv = 0;

    this.hold = 0;
    this.holdCount = 0;
    this.holdPeriod = 1;

    this.dropRemain = 0;
    this.dropBlockRemain = 0;
    this.dropTarget = 1;
    this.dropGain = 1;

    this.humPhase = 0;
    this.lineNoise = 0;
    this.carbonNoise = 0;
    this.duplexGain = 1;
    this.duplexHold = 0;
    this.speakerLow = 0;
    this.speakerBand = 0;
    this.previousSignal = 0;
    this.codecPreviousInput = 0;
    this.codecPreviousOutput = 0;
    this.tonePhase = 0;
    this.tonePhase2 = 0;
    this.warblePhase = 0;

    this.port.onmessage = (ev) => {
      const msg = ev.data;
      if (msg?.type === "reset") {
        this.prng = new XorShift32((msg.seed ?? 0) >>> 0);
        this.env = 0;
        this.limEnv = 0;
        this.hold = 0;
        this.holdCount = 0;
        this.holdPeriod = 1;
        this.dropRemain = 0;
        this.dropBlockRemain = 0;
        this.dropTarget = 1;
        this.dropGain = 1;
        this.humPhase = 0;
        this.lineNoise = 0;
        this.carbonNoise = 0;
        this.duplexGain = 1;
        this.duplexHold = 0;
        this.speakerLow = 0;
        this.speakerBand = 0;
        this.previousSignal = 0;
        this.codecPreviousInput = 0;
        this.codecPreviousOutput = 0;
        this.tonePhase = 0;
        this.tonePhase2 = 0;
        this.warblePhase = 0;
      }
    };
  }

  process(inputs, outputs, parameters) {
    const output = outputs[0];
    const out = output[0];
    if (!out) return true;

    const in0 = inputs[0]?.[0];
    const in1 = inputs[0]?.[1];

    const drive = clamp(parameters.drive[0] ?? 0.35, 0, 1);
    const comp = clamp(parameters.comp[0] ?? 0.45, 0, 1);
    const bits = Math.round(clamp(parameters.bits[0] ?? 12, 4, 16));
    const rateParam = clamp(parameters.rate[0] ?? 24000, 6000, 48000);
    const packet = clamp(parameters.packet[0] ?? 0.2, 0, 1);
    const packetMs = clamp(parameters.packetMs[0] ?? 28, 8, 160);
    const hum = clamp(parameters.hum[0] ?? 0.25, 0, 1);
    const hiss = clamp(parameters.hiss[0] ?? 0.22, 0, 1);
    const toneMix = clamp(parameters.toneMix[0] ?? 0.35, 0, 1);
    const transducer = clamp(parameters.transducer[0] ?? 0.45, 0, 1);
    const lineAge = clamp(parameters.lineAge[0] ?? 0.2, 0, 1);
    const duplex = clamp(parameters.duplex[0] ?? 0.08, 0, 1);
    const speakerRattle = clamp(parameters.speakerRattle[0] ?? 0.12, 0, 1);
    const alarm = (parameters.alarm[0] ?? 0) >= 0.5;
    const mode = Math.round(clamp(parameters.mode[0] ?? 0, 0, 4));
    const ceiling = clamp(parameters.ceiling[0] ?? 0.92, 0.2, 1);
    const outGain = clamp(parameters.outGain[0] ?? 0.95, 0, 1.5);

    const sr = sampleRate;
    // Telecom gain riding should make speech unnervingly present without
    // turning the Drive control into a disguised volume knob.
    const target = mode === 3 ? 0.22 : mode === 2 ? 0.19 : 0.17;
    const envAtk = Math.exp(-1 / ((mode === 3 ? 0.003 : 0.008) * sr));
    const envRel = Math.exp(-1 / ((mode === 2 ? 0.22 : 0.14) * sr));
    const baseDrive = 1 + drive * (mode === 3 ? 7 : mode === 2 ? 5.5 : 4.5);
    const asym = (mode === 0 || mode === 2 ? 0.045 : 0.018) * drive;
    const reference = 0.18;
    const levelMatch = reference / Math.max(1e-5, Math.tanh(reference * baseDrive));

    const qLevels = Math.max(1, (1 << (bits - 1)) - 1);
    const rate = Math.min(sr, rateParam);
    const basePeriod = Math.max(1, Math.round(sr / rate));

    const packetSamples = Math.max(1, Math.round((packetMs / 1000) * sr));
    const edgeFade = 48;
    const dropSlew = 1 / edgeFade;

    const humHz = mode === 1 ? 180 : mode === 2 ? 50 : 60;
    const humDepth = hum * hum * (0.006 + lineAge * 0.018) * (mode === 1 ? 0.45 : 1);
    const hissDepth = hiss * hiss * (0.009 + lineAge * 0.022);

    const duplexModeScale = mode === 2 ? 1 : mode === 1 ? 0.45 : mode === 3 ? 0.22 : mode === 0 ? 0.12 : 0.05;
    const duplexAmount = duplex * duplexModeScale;
    const gateThreshold = 0.0015 + duplexAmount * duplexAmount * 0.035;
    const gateFloor = 1 - duplexAmount * 0.94;
    const gateAttack = 1 - Math.exp(-1 / (0.0025 * sr));
    const gateRelease = 1 - Math.exp(-1 / ((0.035 + duplexAmount * 0.12) * sr));
    const gateHoldSamples = Math.round((0.018 + duplexAmount * 0.075) * sr);

    const speakerHz = mode === 0 ? 920 : mode === 1 ? 2550 : mode === 2 ? 720 : mode === 3 ? 470 : 1080;
    const speakerF = 2 * Math.sin((Math.PI * speakerHz) / sr);
    const speakerDamp = mode === 3 ? 0.34 : mode === 2 ? 0.28 : 0.46;
    const rattleAmount = speakerRattle * (0.25 + transducer * 0.75);

    const warbleRate = mode === 4 ? 2.1 : 2.7;
    const toneBaseA = mode === 4 ? 960 : 880;
    const toneBaseB = mode === 4 ? 1400 : 1200;
    const toneDepth = alarm ? (0.14 + 0.25 * toneMix) : 0;

    for (let i = 0; i < out.length; i++) {
      const x = in0 ? (in1 ? 0.5 * (in0[i] + in1[i]) : in0[i]) : 0;

      // Envelope follower (abs) for AGC.
      const a = Math.abs(x);
      const c = a > this.env ? envAtk : envRel;
      this.env = a + c * (this.env - a);

      const env = this.env + 1e-6;
      const wantGain = clamp(target / env, 0.32, mode === 3 ? 3.2 : 4.5);
      const agc = 1 + (wantGain - 1) * comp;

      // Level-referenced carbon/electronic drive.
      let y = x * agc;
      const center = softClip(asym * baseDrive);
      y = (softClip((y + asym) * baseDrive) - center) * levelMatch;

      // Carbon granules and aging line electronics are excited by the voice,
      // rather than existing as a generic static bed.
      const grainRate = mode === 0 ? 0.18 : mode === 2 ? 0.11 : 0.07;
      this.carbonNoise += (this.prng.nextSigned() - this.carbonNoise) * grainRate;
      const carbonScale = (mode === 0 ? 1 : mode === 2 ? 0.75 : mode === 3 ? 0.42 : 0.18);
      y *= 1 + this.carbonNoise * lineAge * lineAge * carbonScale * 0.24;

      // Sample-rate reduction (ZOH hold) with slight deterministic wobble.
      if (this.holdCount <= 0) {
        const wob = packet * 0.35;
        const j = wob > 0 ? this.prng.nextSigned() * wob : 0;
        this.holdPeriod = Math.max(1, Math.round(basePeriod * (1 + j)));
        this.hold = y;
        this.holdCount = this.holdPeriod;
      }
      y = this.hold;
      this.holdCount--;

      // Device-specific coding. Cell mode uses a primitive predictive codec;
      // copper landlines use companded quantization; speakers use ordinary PCM.
      const codecMix = clamp((14 - bits) / 10 + lineAge * 0.16, 0, 1);
      if (mode === 1) {
        const prediction = this.codecPreviousInput * 0.78;
        const residual = clamp(y - prediction, -1, 1);
        const quantized = Math.round(residual * qLevels) / qLevels;
        const decoded = clamp(quantized + this.codecPreviousOutput * 0.78, -1, 1);
        this.codecPreviousInput = y;
        this.codecPreviousOutput = decoded;
        y = y * (1 - codecMix) + decoded * codecMix;
      } else if (mode === 0) {
        const mu = 31 + lineAge * 224;
        const encoded = Math.sign(y) * Math.log1p(mu * Math.abs(clamp(y, -1, 1))) / Math.log1p(mu);
        const q = Math.round(encoded * qLevels) / qLevels;
        const decoded = Math.sign(q) * Math.expm1(Math.abs(q) * Math.log1p(mu)) / mu;
        y = y * (1 - codecMix) + decoded * codecMix;
      } else {
        const quantized = Math.round(clamp(y, -1, 1) * qLevels) / qLevels;
        y = y * (1 - codecMix) + quantized * codecMix;
      }

      // Packet loss / dropouts in blocks (deterministic via PRNG seed).
      if (this.dropBlockRemain <= 0) {
        // Make the drop decision at the boundary itself. The old per-quantum
        // initializer silently skipped every decision whenever packetSamples
        // was an exact multiple of the browser's 128-sample render quantum.
        this.dropBlockRemain = packetSamples;
        const p = packet * packet;
        const doDrop = this.prng.nextFloat() < p;
        this.dropRemain = doDrop ? packetSamples : 0;
      }
      this.dropBlockRemain--;

      this.dropTarget = this.dropRemain > 0 ? 0 : 1;
      const dg = this.dropGain + (this.dropTarget - this.dropGain) * dropSlew;
      this.dropGain = clamp(dg, 0, 1);
      if (this.dropRemain > 0) this.dropRemain--;
      y *= this.dropGain;

      // Intercom talk paths and cheap hands-free phones do not stay perfectly
      // open. This creates the recognisable half-duplex clamp and release.
      if (this.env >= gateThreshold) this.duplexHold = gateHoldSamples;
      else if (this.duplexHold > 0) this.duplexHold--;
      const duplexTarget = this.duplexHold > 0 || this.env >= gateThreshold ? 1 : gateFloor;
      const duplexSlew = duplexTarget > this.duplexGain ? gateAttack : gateRelease;
      this.duplexGain += (duplexTarget - this.duplexGain) * duplexSlew;
      y *= this.duplexGain;

      // A small, signal-excited resonator models receiver diaphragms,
      // intercom boxes and PA horns. It remains silent with silent input.
      this.speakerLow += speakerF * this.speakerBand;
      const speakerHigh = y - this.speakerLow - speakerDamp * this.speakerBand;
      this.speakerBand += speakerF * speakerHigh;
      const transient = y - this.previousSignal;
      this.previousSignal = y;
      const body = clamp(this.speakerBand + transient * (mode === 3 ? 0.55 : 0.25), -1.5, 1.5);
      y += body * rattleAmount * (mode === 3 ? 0.24 : mode === 2 ? 0.2 : 0.12);

      // Add comms noise: hum + hiss.
      this.humPhase += (2 * Math.PI * humHz) / sr;
      if (this.humPhase > Math.PI * 2) this.humPhase -= Math.PI * 2;
      const humSig = (Math.sin(this.humPhase) + 0.38 * Math.sin(this.humPhase * 2.01)) * humDepth;
      this.lineNoise += (this.prng.nextSigned() - this.lineNoise) * (mode === 0 ? 0.32 : 0.52);
      const hissSig = (0.72 * this.lineNoise + 0.28 * this.prng.nextSigned()) * hissDepth;
      y = y + humSig + hissSig;

      // Optional alarm tone overlay (warbling 2-tone).
      if (alarm && toneMix > 0.0001) {
        this.warblePhase += (2 * Math.PI * warbleRate) / sr;
        if (this.warblePhase > Math.PI * 2) this.warblePhase -= Math.PI * 2;
        const w = 0.5 + 0.5 * Math.sin(this.warblePhase);
        const f1 = toneBaseA * (0.96 + 0.08 * w);
        const f2 = toneBaseB * (1.03 - 0.06 * w);
        this.tonePhase += (2 * Math.PI * f1) / sr;
        this.tonePhase2 += (2 * Math.PI * f2) / sr;
        if (this.tonePhase > Math.PI * 2) this.tonePhase -= Math.PI * 2;
        if (this.tonePhase2 > Math.PI * 2) this.tonePhase2 -= Math.PI * 2;
        const tone = (Math.sin(this.tonePhase) + 0.6 * Math.sin(this.tonePhase2)) * toneDepth;
        y = y * (1 - 0.55 * toneMix) + tone * (0.55 * toneMix) + y * (0.12 * toneMix);
      }

      // Output gain + soft limiter to ceiling.
      let post = y * outGain;
      const aa = Math.abs(post);
      const limAtk = Math.exp(-1 / (0.002 * sr));
      const limRel = Math.exp(-1 / (0.06 * sr));
      const lc = aa > this.limEnv ? limAtk : limRel;
      this.limEnv = aa + lc * (this.limEnv - aa);
      const g = this.limEnv > ceiling ? ceiling / (this.limEnv + 1e-6) : 1;
      post = post * g;

      out[i] = clamp(post, -ceiling, ceiling);
    }

    return true;
  }
}

registerProcessor("comms", CommsProcessor);

