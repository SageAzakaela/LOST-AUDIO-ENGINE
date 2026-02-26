const WORKLET_URL = new URL("./conference-processor.js", import.meta.url);

function now(ctx) {
  return ctx.currentTime;
}

export async function ensureWorklet(ctx) {
  if (ctx.__conferenceWorkletLoaded) return;
  await ctx.audioWorklet.addModule(WORKLET_URL.href);
  ctx.__conferenceWorkletLoaded = true;
}

export function defaultSettings() {
  return {
    mode: "discord",
    bandwidth: 0.45,
    codec: 0.35,
    dropouts: 0.25,
    jitter: 0.2,
    robot: 0.12,
    noise: 0.12,

    hpHz: 260,
    lpHz: 4200,
    midHumpDb: 2.2,
    midFreq: 1750,

    concealMode: "hold",
    packetLoss: 0.18,
    packetMs: 24,
    repeatMs: 42,
    jitterMs: 0.12,
    jitterRate: 34,
    gate: 0.12,
    bits: 12,
    rate: 24000,
    ceiling: 0.92,
    outGain: 0.98,
  };
}

function modeToIndex(mode) {
  if (typeof mode === "number") return Math.round(Math.max(0, Math.min(3, mode)));
  if (mode === "zoom") return 1;
  if (mode === "skype") return 2;
  if (mode === "cell") return 3;
  return 0; // discord
}

function concealToIndex(mode) {
  if (typeof mode === "number") return Math.round(Math.max(0, Math.min(3, mode)));
  if (mode === "mute") return 1;
  if (mode === "interp") return 2;
  if (mode === "repeat") return 3;
  return 0; // hold
}

export async function buildConferenceGraph(ctx, { seed }) {
  await ensureWorklet(ctx);

  const input = new GainNode(ctx, { gain: 1 });

  const hp1 = new BiquadFilterNode(ctx, { type: "highpass", Q: 0.707, frequency: 260 });
  const hp2 = new BiquadFilterNode(ctx, { type: "highpass", Q: 0.707, frequency: 260 });
  const dip = new BiquadFilterNode(ctx, { type: "peaking", frequency: 650, Q: 0.9, gain: -0.8 });
  const hump = new BiquadFilterNode(ctx, { type: "peaking", frequency: 1750, Q: 1.25, gain: 2.2 });
  const lp1 = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.85, frequency: 4200 });
  const lp2 = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.85, frequency: 4200 });

  const processor = new AudioWorkletNode(ctx, "conference", {
    numberOfInputs: 1,
    numberOfOutputs: 1,
    outputChannelCount: [1],
    processorOptions: { seed: seed >>> 0 },
  });

  const out = new GainNode(ctx, { gain: 1 });

  input.connect(hp1);
  hp1.connect(hp2);
  hp2.connect(dip);
  dip.connect(hump);
  hump.connect(lp1);
  lp1.connect(lp2);
  lp2.connect(processor);
  processor.connect(out);

  function reset(seedNext) {
    processor.port.postMessage({ type: "reset", seed: seedNext >>> 0 });
  }

  function applySettings(settings, { time = now(ctx), ramp = 0.02 } = {}) {
    const t1 = time + ramp;
    const s = { ...settings };

    const hpHz = Math.max(40, Math.min(1200, s.hpHz ?? 260));
    const lpHz = Math.max(800, Math.min(16000, s.lpHz ?? 4200));
    const midDb = Math.max(0, Math.min(14, s.midHumpDb ?? 2.2));
    const midF = Math.max(600, Math.min(5000, s.midFreq ?? 1750));

    for (const h of [hp1, hp2]) {
      h.frequency.cancelScheduledValues(time);
      h.frequency.setValueAtTime(h.frequency.value, time);
      h.frequency.linearRampToValueAtTime(hpHz, t1);
    }
    for (const l of [lp1, lp2]) {
      l.frequency.cancelScheduledValues(time);
      l.frequency.setValueAtTime(l.frequency.value, time);
      l.frequency.linearRampToValueAtTime(lpHz, t1);
    }

    hump.frequency.cancelScheduledValues(time);
    hump.frequency.setValueAtTime(hump.frequency.value, time);
    hump.frequency.linearRampToValueAtTime(midF, t1);
    hump.gain.cancelScheduledValues(time);
    hump.gain.setValueAtTime(hump.gain.value, time);
    hump.gain.linearRampToValueAtTime(midDb, t1);

    dip.gain.cancelScheduledValues(time);
    dip.gain.setValueAtTime(dip.gain.value, time);
    dip.gain.linearRampToValueAtTime(-0.35 * midDb, t1);

    processor.parameters.get("mode")?.setValueAtTime(modeToIndex(s.mode), time);
    processor.parameters.get("concealMode")?.setValueAtTime(concealToIndex(s.concealMode), time);
    processor.parameters.get("packetLoss")?.setValueAtTime(s.packetLoss ?? 0.18, time);
    processor.parameters.get("packetMs")?.setValueAtTime(s.packetMs ?? 24, time);
    processor.parameters.get("repeatMs")?.setValueAtTime(s.repeatMs ?? 42, time);
    processor.parameters.get("jitterMs")?.setValueAtTime(s.jitterMs ?? 0.12, time);
    processor.parameters.get("jitterRate")?.setValueAtTime(s.jitterRate ?? 34, time);
    processor.parameters.get("gate")?.setValueAtTime(s.gate ?? 0.12, time);
    processor.parameters.get("bits")?.setValueAtTime(s.bits ?? 12, time);
    processor.parameters.get("rate")?.setValueAtTime(s.rate ?? 24000, time);
    processor.parameters.get("robot")?.setValueAtTime(s.robot ?? 0.12, time);
    processor.parameters.get("noise")?.setValueAtTime(s.noise ?? 0.12, time);
    processor.parameters.get("ceiling")?.setValueAtTime(s.ceiling ?? 0.92, time);
    processor.parameters.get("outGain")?.setValueAtTime(s.outGain ?? 0.98, time);
  }

  return { input, output: out, nodes: { input, hp1, hp2, dip, hump, lp1, lp2, processor, out }, reset, applySettings };
}

