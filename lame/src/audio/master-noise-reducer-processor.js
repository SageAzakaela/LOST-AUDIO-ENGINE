class MasterNoiseReducerProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: "thresholdDb", defaultValue: -55, minValue: -90, maxValue: -10, automationRate: "k-rate" },
      { name: "reductionDb", defaultValue: 12, minValue: 0, maxValue: 48, automationRate: "k-rate" },
      { name: "attackMs", defaultValue: 12, minValue: 1, maxValue: 200, automationRate: "k-rate" },
      { name: "releaseMs", defaultValue: 220, minValue: 10, maxValue: 2000, automationRate: "k-rate" },
      { name: "mix", defaultValue: 0, minValue: 0, maxValue: 1, automationRate: "k-rate" },
      { name: "learn", defaultValue: 0, minValue: 0, maxValue: 1, automationRate: "k-rate" },
    ];
  }

  constructor() {
    super();
    this.envelope = [];
    this.gain = [];
    this.noiseFloorDb = -72;
    this.reportCounter = 0;
  }

  process(inputs, outputs, parameters) {
    const input = inputs[0] || [];
    const output = outputs[0] || [];
    const thresholdDb = parameters.thresholdDb[0];
    const reductionDb = parameters.reductionDb[0];
    const attackMs = parameters.attackMs[0];
    const releaseMs = parameters.releaseMs[0];
    const mix = parameters.mix[0];
    const learning = parameters.learn[0] >= 0.5;
    const envelopeAttack = Math.exp(-1 / (sampleRate * 0.006));
    const envelopeRelease = Math.exp(-1 / (sampleRate * 0.09));
    const closeCoefficient = Math.exp(-1 / (sampleRate * Math.max(0.001, attackMs / 1000)));
    const openCoefficient = Math.exp(-1 / (sampleRate * Math.max(0.001, releaseMs / 1000)));
    let blockFloor = 0;

    for (let channel = 0; channel < output.length; channel++) {
      const source = input[channel] || input[0];
      const destination = output[channel];
      if (!source) {
        destination.fill(0);
        continue;
      }
      let envelope = this.envelope[channel] || 0;
      let gain = this.gain[channel] ?? 1;
      for (let index = 0; index < destination.length; index++) {
        const sample = source[index] || 0;
        const absolute = Math.abs(sample);
        const envCoefficient = absolute > envelope ? envelopeAttack : envelopeRelease;
        envelope = absolute + envCoefficient * (envelope - absolute);
        const levelDb = 20 * Math.log10(Math.max(1e-8, envelope));
        blockFloor += levelDb;
        const effectiveThreshold = learning ? Math.max(thresholdDb, this.noiseFloorDb + 4) : thresholdDb;
        const depth = Math.max(0, effectiveThreshold - levelDb);
        const attenuationDb = -Math.min(reductionDb, depth * 1.5);
        const targetGain = Math.pow(10, attenuationDb / 20);
        const coefficient = targetGain < gain ? closeCoefficient : openCoefficient;
        gain = targetGain + coefficient * (gain - targetGain);
        destination[index] = sample * ((1 - mix) + gain * mix);
      }
      this.envelope[channel] = envelope;
      this.gain[channel] = gain;
    }

    const sampleCount = Math.max(1, output.length * (output[0]?.length || 0));
    if (learning) {
      const observed = blockFloor / sampleCount;
      if (Number.isFinite(observed)) this.noiseFloorDb += (Math.min(-18, Math.max(-90, observed)) - this.noiseFloorDb) * 0.003;
    }
    this.reportCounter += output[0]?.length || 128;
    if (this.reportCounter >= sampleRate / 8) {
      this.reportCounter = 0;
      this.port.postMessage({ type: "noiseFloor", value: this.noiseFloorDb });
    }
    return true;
  }
}

registerProcessor("master-noise-reducer", MasterNoiseReducerProcessor);
