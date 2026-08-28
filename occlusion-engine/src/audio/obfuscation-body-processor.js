/* eslint-disable no-undef */
class XorShift32 {
  constructor(seed) {
    this.state = (seed >>> 0) || 0x6f626675;
  }
  next() {
    let x = this.state >>> 0;
    x ^= x << 13;
    x ^= x >>> 17;
    x ^= x << 5;
    this.state = x >>> 0;
    return this.state / 0xffffffff;
  }
}

function clamp(x, lo, hi) {
  return Math.min(hi, Math.max(lo, x));
}

function smoothstep(x) {
  const t = clamp(x, 0, 1);
  return t * t * (3 - 2 * t);
}

class ObfuscationBodyProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: "amount", defaultValue: 0, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "looseness", defaultValue: 0.25, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "brightness", defaultValue: 0.5, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "bodyHz", defaultValue: 220, minValue: 40, maxValue: 6000, automationRate: "k-rate" },
      { name: "cavityHz", defaultValue: 420, minValue: 50, maxValue: 8000, automationRate: "k-rate" },
    ];
  }

  constructor(options) {
    super();
    this.prng = new XorShift32(options?.processorOptions?.seed ?? 0x6f626675);
    this.env = 0;
    this.previousAbs = 0;
    this.previousInput = 0;
    this.burst = 0;
    this.cooldown = 0;
    this.filters = [
      { z1: 0, z2: 0 },
      { z1: 0, z2: 0 },
      { z1: 0, z2: 0 },
    ];
    this.port.onmessage = (event) => {
      if (event.data?.type !== "reset") return;
      this.prng = new XorShift32(event.data.seed ?? 0x6f626675);
      this.env = 0;
      this.previousAbs = 0;
      this.previousInput = 0;
      this.burst = 0;
      this.cooldown = 0;
      for (const filter of this.filters) {
        filter.z1 = 0;
        filter.z2 = 0;
      }
    };
  }

  coefficients(frequency, q) {
    const hz = clamp(frequency, 30, sampleRate * 0.42);
    const w0 = (2 * Math.PI * hz) / sampleRate;
    const alpha = Math.sin(w0) / (2 * Math.max(0.5, q));
    const a0 = 1 + alpha;
    return {
      b0: alpha / a0,
      b1: 0,
      b2: -alpha / a0,
      a1: (-2 * Math.cos(w0)) / a0,
      a2: (1 - alpha) / a0,
    };
  }

  runFilter(index, input, coefficients) {
    const state = this.filters[index];
    const output = coefficients.b0 * input + state.z1;
    state.z1 = coefficients.b1 * input - coefficients.a1 * output + state.z2;
    state.z2 = coefficients.b2 * input - coefficients.a2 * output;
    return output;
  }

  process(inputs, outputs, parameters) {
    const output = outputs[0]?.[0];
    if (!output) return true;
    const input = inputs[0]?.[0];
    const amount = clamp(parameters.amount[0] ?? 0, 0, 1);
    const looseness = clamp(parameters.looseness[0] ?? 0.25, 0, 1);
    const brightness = clamp(parameters.brightness[0] ?? 0.5, 0, 1);
    const bodyHz = clamp(parameters.bodyHz[0] ?? 220, 40, sampleRate * 0.3);
    const cavityHz = clamp(parameters.cavityHz[0] ?? 420, 50, sampleRate * 0.34);
    const q = 3.5 + looseness * 18;
    const c0 = this.coefficients(bodyHz, q * 0.72);
    const c1 = this.coefficients(cavityHz, q);
    const c2 = this.coefficients(Math.min(sampleRate * 0.38, cavityHz * (1.65 + brightness * 1.5)), q * 1.15);
    const envRelease = Math.exp(-1 / (sampleRate * (0.035 + looseness * 0.12)));
    const burstRelease = Math.exp(-1 / (sampleRate * (0.006 + looseness * 0.055)));
    const threshold = 0.012 + (1 - looseness) * 0.065;

    for (let i = 0; i < output.length; i++) {
      const x = input?.[i] || 0;
      const ax = Math.abs(x);
      this.env = Math.max(ax, this.env * envRelease);
      const onset = Math.max(0, ax - this.previousAbs * 0.94);
      const contact = smoothstep((this.env - threshold) / Math.max(0.025, 0.22 - threshold));

      if (this.cooldown > 0) this.cooldown--;
      const triggerThreshold = 0.003 + (1 - looseness) * 0.01;
      if (amount > 0.0001 && contact > 0 && onset > triggerThreshold && this.cooldown <= 0) {
        const probability = 0.16 + looseness * 0.72;
        if (this.prng.next() < probability) {
          const polarity = this.prng.next() < 0.5 ? -1 : 1;
          this.burst += polarity * onset * (0.6 + looseness * 2.4);
          this.cooldown = Math.floor(sampleRate * (0.002 + (1 - looseness) * 0.018));
        }
      }

      const derivative = x - this.previousInput;
      const contactBuzz = Math.tanh(derivative * (7 + looseness * 25)) * contact * (0.012 + looseness * 0.05);
      // A resonant panel is driven by sustained pressure as well as impacts.
      // This low-level flex term keeps wood, glass and sheet metal alive under
      // speech/tones while the burst path still supplies loose-hardware chatter.
      const panelFlex = Math.tanh(x * (1.8 + brightness * 2.8)) * contact * (0.018 + (1 - looseness) * 0.018);
      const excitation = this.burst + contactBuzz + panelFlex;
      this.burst *= burstRelease;

      const lowBody = this.runFilter(0, excitation, c0);
      const cavity = this.runFilter(1, excitation, c1);
      const metallic = this.runFilter(2, excitation, c2);
      const rattle = lowBody * 0.72 + cavity * (0.8 - brightness * 0.2) + metallic * (0.2 + brightness * 0.62);
      output[i] = 0.3 * Math.tanh((rattle * amount * 2.6) / 0.3);

      this.previousAbs = ax;
      this.previousInput = x;
    }
    return true;
  }
}

registerProcessor("obfuscation-body", ObfuscationBodyProcessor);
