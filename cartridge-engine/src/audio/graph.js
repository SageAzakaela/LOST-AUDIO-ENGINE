const WORKLET_URL = new URL("./cartridge-processor.js", import.meta.url);

function now(ctx) {
  return ctx.currentTime;
}

export async function ensureWorklet(ctx) {
  if (ctx.__cartridgeWorkletLoaded) return;
  await ctx.audioWorklet.addModule(WORKLET_URL.href);
  ctx.__cartridgeWorkletLoaded = true;
}

export function defaultSettings() {
  return {
    quality: 0.55,
    codec: 0.25,
    grit: 0.25,
    noise: 0.15,

    bleepsEnable: false,
    bleepsMix: 0.18,
    bleepsRate: 3,
    bleepsWave: "random",
    bleepsVibrato: 0.35,
    bleepsPitch: 0.55,

    microDelayMs: 8,
    microDelayMix: 0.18,
    verb: 0.22,
    verbMs: 45,
    wet: 1,
    ceiling: 0.92,
    limiter: 0.35,
    edge: 0.25,
    noiseTrack: 0.6,
    dcDrift: 0.15,
    hpHz: 70,
    speaker: 0.45,

    bits: 10,
    rate: 24000,
    jitter: 0.05,
    dither: true,
    noiseShaping: false,

    lpHz: 9000,
    preEmph: 0.2,
    mulaw: 0.25,
    blockMs: 8,

    sat: 0.25,
    hum: 0.08,
    whine: 0.15,
    outGain: 0.95,
  };
}

export async function buildCartridgeGraph(ctx, { seed }) {
  await ensureWorklet(ctx);
  const bleepNode = new AudioWorkletNode(ctx, "bleep-seq", {
    numberOfInputs: 0,
    numberOfOutputs: 1,
    outputChannelCount: [1],
    processorOptions: { seed: (seed ^ 0x0b1ee0f5) >>> 0 },
  });
  const bleepGain = new GainNode(ctx, { gain: 0 });
  const mix = new GainNode(ctx, { gain: 1 });

  const processor = new AudioWorkletNode(ctx, "cartridge", {
    numberOfInputs: 1,
    numberOfOutputs: 1,
    outputChannelCount: [1],
    processorOptions: { seed: seed >>> 0 },
  });

  const lp = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.707, frequency: 9000 });
  const hp = new BiquadFilterNode(ctx, { type: "highpass", Q: 0.707, frequency: 70 });
  const dip = new BiquadFilterNode(ctx, { type: "peaking", frequency: 650, Q: 0.9, gain: 0 });
  const hump = new BiquadFilterNode(ctx, { type: "peaking", frequency: 1900, Q: 1.2, gain: 0 });
  const spLp1 = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.85, frequency: 7000 });
  const spLp2 = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.85, frequency: 7000 });
  const out = new GainNode(ctx, { gain: 1 });

  // Mix source + bleeps, then LP pre-degradation; keeps post-DAC hum/whine/noise audible.
  mix.connect(lp);
  bleepNode.connect(bleepGain);
  bleepGain.connect(mix);
  lp.connect(processor);
  processor.connect(hp);
  hp.connect(dip);
  dip.connect(hump);
  hump.connect(spLp1);
  spLp1.connect(spLp2);
  spLp2.connect(out);

  function reset(seedNext) {
    processor.port.postMessage({ type: "reset", seed: seedNext >>> 0 });
    bleepNode.port.postMessage({ type: "reset", seed: ((seedNext >>> 0) ^ 0x0b1ee0f5) >>> 0 });
  }

  function applySettings(settings, { time = now(ctx), ramp = 0.02 } = {}) {
    const t1 = time + ramp;
    const s = { ...settings };

    lp.frequency.cancelScheduledValues(time);
    lp.frequency.setValueAtTime(lp.frequency.value, time);
    lp.frequency.linearRampToValueAtTime(Math.max(2500, Math.min(18000, s.lpHz ?? 9000)), t1);

    hp.frequency.cancelScheduledValues(time);
    hp.frequency.setValueAtTime(hp.frequency.value, time);
    hp.frequency.linearRampToValueAtTime(Math.max(20, Math.min(240, s.hpHz ?? 70)), t1);

    const spk = Math.min(1, Math.max(0, s.speaker ?? 0.45));
    dip.gain.cancelScheduledValues(time);
    dip.gain.setValueAtTime(dip.gain.value, time);
    dip.gain.linearRampToValueAtTime(-spk * 4.5, t1);
    hump.gain.cancelScheduledValues(time);
    hump.gain.setValueAtTime(hump.gain.value, time);
    hump.gain.linearRampToValueAtTime(spk * 6.5, t1);
    const spkLp = 12000 - spk * 9000; // 12k -> 3k
    for (const l of [spLp1, spLp2]) {
      l.frequency.cancelScheduledValues(time);
      l.frequency.setValueAtTime(l.frequency.value, time);
      l.frequency.linearRampToValueAtTime(spkLp, t1);
    }

    processor.parameters.get("bits")?.setValueAtTime(s.bits ?? 10, time);
    processor.parameters.get("rate")?.setValueAtTime(s.rate ?? 24000, time);
    processor.parameters.get("jitter")?.setValueAtTime(s.jitter ?? 0.05, time);
    processor.parameters.get("dither")?.setValueAtTime(s.dither ? 1 : 0, time);
    processor.parameters.get("noiseShaping")?.setValueAtTime(s.noiseShaping ? 1 : 0, time);

    processor.parameters.get("preEmph")?.setValueAtTime(s.preEmph ?? 0.2, time);
    processor.parameters.get("mulaw")?.setValueAtTime(s.mulaw ?? 0.25, time);
    processor.parameters.get("blockMs")?.setValueAtTime(s.blockMs ?? 8, time);

    processor.parameters.get("sat")?.setValueAtTime(s.sat ?? 0.25, time);
    processor.parameters.get("edge")?.setValueAtTime(s.edge ?? 0.25, time);
    processor.parameters.get("dcDrift")?.setValueAtTime(s.dcDrift ?? 0.15, time);
    processor.parameters.get("hum")?.setValueAtTime(s.hum ?? 0.08, time);
    processor.parameters.get("whine")?.setValueAtTime(s.whine ?? 0.15, time);
    processor.parameters.get("noise")?.setValueAtTime(s.noise ?? 0.15, time);
    processor.parameters.get("noiseTrack")?.setValueAtTime(s.noiseTrack ?? 0.6, time);

    processor.parameters.get("microDelayMs")?.setValueAtTime(s.microDelayMs ?? 8, time);
    processor.parameters.get("microDelayMix")?.setValueAtTime(s.microDelayMix ?? 0.18, time);
    processor.parameters.get("verb")?.setValueAtTime(s.verb ?? 0.22, time);
    processor.parameters.get("verbMs")?.setValueAtTime(s.verbMs ?? 45, time);

    processor.parameters.get("limiter")?.setValueAtTime(s.limiter ?? 0.35, time);
    processor.parameters.get("ceiling")?.setValueAtTime(s.ceiling ?? 0.92, time);
    processor.parameters.get("wet")?.setValueAtTime(s.wet ?? 1, time);

    processor.parameters.get("outGain")?.setValueAtTime(s.outGain ?? 0.95, time);

    const waveMap = s.bleepsWave === "pulse" ? 1 : s.bleepsWave === "saw" ? 2 : s.bleepsWave === "tri" ? 3 : 0;
    bleepNode.parameters.get("enable")?.setValueAtTime(s.bleepsEnable ? 1 : 0, time);
    bleepNode.parameters.get("mix")?.setValueAtTime(s.bleepsMix ?? 0.18, time);
    bleepNode.parameters.get("rate")?.setValueAtTime(s.bleepsRate ?? 3, time);
    bleepNode.parameters.get("wave")?.setValueAtTime(waveMap, time);
    bleepNode.parameters.get("vibrato")?.setValueAtTime(s.bleepsVibrato ?? 0.35, time);
    bleepNode.parameters.get("pitch")?.setValueAtTime(s.bleepsPitch ?? 0.55, time);

    bleepGain.gain.setValueAtTime(1, time);
  }

  return { input: mix, output: out, nodes: { bleepNode, bleepGain, mix, processor, lp, hp, dip, hump, spLp1, spLp2, out }, reset, applySettings };
}
