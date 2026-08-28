const WORKLET_URL = new URL("./cartridge-processor.js?v=20260827.26", import.meta.url);

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
    noise: 0.08,

    bleepsEnable: false,
    bleepsMix: 0.12,
    bleepsRate: 3,
    bleepsWave: "pulse",
    bleepsTrigger: "transient",
    bleepsScale: "minor",
    bleepsVibrato: 0.12,
    bleepsPitch: 0.55,

    microDelayMs: 8,
    microDelayMix: 0.06,
    verb: 0.08,
    verbMs: 45,
    wet: 1,
    ceiling: 0.92,
    limiter: 0.35,
    edge: 0.25,
    noiseTrack: 0.6,
    dcDrift: 0.15,
    hpHz: 70,
    speaker: 0.45,
    speakerModel: "handheld",

    bits: 10,
    rate: 24000,
    jitter: 0.05,
    dither: true,
    noiseShaping: false,

    lpHz: 9000,
    preEmph: 0.2,
    mulaw: 0.25,
    codecMode: "adpcm",
    blockMs: 8,

    sat: 0.25,
    hum: 0.08,
    whine: 0.15,
    outGain: 0.95,
  };
}

export async function buildCartridgeGraph(ctx, { seed }) {
  await ensureWorklet(ctx);
  const input = new GainNode(ctx, { gain: 1 });
  const bleepNode = new AudioWorkletNode(ctx, "bleep-seq", {
    numberOfInputs: 1,
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

  // The chip voice listens to source transients and is then degraded through
  // the same sample-memory/DAC path as the source.
  input.connect(mix);
  input.connect(bleepNode);
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

    const speakerProfiles = {
      direct: { hp: 35, lp: 15000, dipHz: 720, dipDb: 0, humpHz: 1800, humpDb: 0, q: 1 },
      handheld: { hp: 180, lp: 5200, dipHz: 720, dipDb: -3.2, humpHz: 2050, humpDb: 5.2, q: 1.55 },
      television: { hp: 105, lp: 7200, dipHz: 620, dipDb: -2.2, humpHz: 1450, humpDb: 4.1, q: 1.25 },
      cabinet: { hp: 72, lp: 8600, dipHz: 980, dipDb: -1.6, humpHz: 2380, humpDb: 3.8, q: 1.1 },
      pc: { hp: 360, lp: 4300, dipHz: 900, dipDb: -4.5, humpHz: 2350, humpDb: 7.4, q: 2.1 },
    };
    const profile = speakerProfiles[s.speakerModel] ?? speakerProfiles.handheld;
    const spk = Math.min(1, Math.max(0, s.speaker ?? 0.45));
    const manualHp = Math.max(20, Math.min(420, s.hpHz ?? 70));
    const speakerHp = manualHp * (1 - spk) + Math.max(manualHp, profile.hp) * spk;
    hp.frequency.cancelScheduledValues(time);
    hp.frequency.setValueAtTime(hp.frequency.value, time);
    hp.frequency.linearRampToValueAtTime(speakerHp, t1);

    dip.frequency.cancelScheduledValues(time);
    dip.frequency.setValueAtTime(dip.frequency.value, time);
    dip.frequency.linearRampToValueAtTime(profile.dipHz, t1);
    dip.gain.cancelScheduledValues(time);
    dip.gain.setValueAtTime(dip.gain.value, time);
    dip.gain.linearRampToValueAtTime(profile.dipDb * spk, t1);
    hump.frequency.cancelScheduledValues(time);
    hump.frequency.setValueAtTime(hump.frequency.value, time);
    hump.frequency.linearRampToValueAtTime(profile.humpHz, t1);
    hump.Q.cancelScheduledValues(time);
    hump.Q.setValueAtTime(hump.Q.value, time);
    hump.Q.linearRampToValueAtTime(profile.q, t1);
    hump.gain.cancelScheduledValues(time);
    hump.gain.setValueAtTime(hump.gain.value, time);
    hump.gain.linearRampToValueAtTime(profile.humpDb * spk, t1);
    const spkLp = 15000 * (1 - spk) + profile.lp * spk;
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
    const codecMap = { pcm: 0, dpcm: 1, adpcm: 2, brr: 3, mulaw: 4 };
    processor.parameters.get("codecMode")?.setValueAtTime(codecMap[s.codecMode] ?? 2, time);
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

    const waveMap = s.bleepsWave === "pulse" ? 1 : s.bleepsWave === "saw" ? 2 : s.bleepsWave === "tri" ? 3 : s.bleepsWave === "noise" ? 4 : 0;
    const triggerMap = { transient: 0, clock: 1, hybrid: 2 };
    const scaleMap = { pentatonic: 0, minor: 1, major: 2, chromatic: 3 };
    bleepNode.parameters.get("enable")?.setValueAtTime(s.bleepsEnable ? 1 : 0, time);
    bleepNode.parameters.get("mix")?.setValueAtTime(s.bleepsMix ?? 0.12, time);
    bleepNode.parameters.get("rate")?.setValueAtTime(s.bleepsRate ?? 3, time);
    bleepNode.parameters.get("wave")?.setValueAtTime(waveMap, time);
    bleepNode.parameters.get("trigger")?.setValueAtTime(triggerMap[s.bleepsTrigger] ?? 0, time);
    bleepNode.parameters.get("scale")?.setValueAtTime(scaleMap[s.bleepsScale] ?? 1, time);
    bleepNode.parameters.get("vibrato")?.setValueAtTime(s.bleepsVibrato ?? 0.12, time);
    bleepNode.parameters.get("pitch")?.setValueAtTime(s.bleepsPitch ?? 0.55, time);

    bleepGain.gain.setValueAtTime(1, time);
  }

  return { input, output: out, nodes: { input, bleepNode, bleepGain, mix, processor, lp, hp, dip, hump, spLp1, spLp2, out }, reset, applySettings };
}
