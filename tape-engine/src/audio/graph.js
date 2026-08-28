const WORKLET_URL = new URL("./tape-processor.js?v=20260827.21", import.meta.url);
const TAPE_LATENCY_SECONDS = 0.012;

function now(ctx) {
  return ctx.currentTime;
}

export async function ensureWorklet(ctx) {
  if (ctx.__tapeWorkletLoaded) return;
  await ctx.audioWorklet.addModule(WORKLET_URL.href);
  ctx.__tapeWorkletLoaded = true;
}

export function defaultSettings() {
  return {
    quality: 0.55,
    age: 0.35,
    wow: 0.25,
    glitch: 0.18,
    sfxEnable: true,

    hpHz: 35,
    lpHz: 11000,
    headBumpDb: 2.2,
    headBumpHz: 85,
    drive: 0.35,
    comp: 0.28,
    speed: 1,
    wowDepthMs: 3.5,
    flutterDepthMs: 1.2,
    hiss: 0.12,
    hum: 0.05,
    dropout: 0.18,
    dropoutMs: 38,
    ceiling: 0.92,
    outGain: 0.98,

    sfxSource: "cassette",
    sfxLevel: 0.46,
    sfxMode: "bed",
  };
}

export async function buildTapeGraph(ctx, { seed, sfxBuffer = null } = {}) {
  await ensureWorklet(ctx);

  const input = new GainNode(ctx, { gain: 1 });
  const sfxGain = new GainNode(ctx, { gain: 0 });
  const mix = new GainNode(ctx, { gain: 1 });

  const hp = new BiquadFilterNode(ctx, { type: "highpass", Q: 0.707, frequency: 35 });
  const bump = new BiquadFilterNode(ctx, { type: "peaking", Q: 0.9, frequency: 85, gain: 2.2 });
  const lp1 = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.85, frequency: 11000 });
  const lp2 = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.85, frequency: 11000 });

  const processor = new AudioWorkletNode(ctx, "tape", {
    numberOfInputs: 1,
    numberOfOutputs: 1,
    outputChannelCount: [1],
    processorOptions: { seed: seed >>> 0 },
  });

  const out = new GainNode(ctx, { gain: 1 });

  // Source + optional SFX into the tape processor, then EQ.
  input.connect(mix);
  sfxGain.connect(mix);
  mix.connect(processor);
  processor.connect(hp);
  hp.connect(bump);
  bump.connect(lp1);
  lp1.connect(lp2);
  lp2.connect(out);

  function reset(seedNext) {
    processor.port.postMessage({ type: "reset", seed: seedNext >>> 0 });
  }

  function applySettings(settings, { time = now(ctx), ramp = 0.02 } = {}) {
    const t1 = time + ramp;
    const s = { ...settings };

    sfxGain.gain.cancelScheduledValues(time);
    sfxGain.gain.setValueAtTime(sfxGain.gain.value, time);
    sfxGain.gain.linearRampToValueAtTime(s.sfxEnable ? Math.max(0, Math.min(1, s.sfxLevel ?? 0.46)) : 0, t1);

    hp.frequency.cancelScheduledValues(time);
    hp.frequency.setValueAtTime(hp.frequency.value, time);
    hp.frequency.linearRampToValueAtTime(Math.max(10, Math.min(240, s.hpHz ?? 35)), t1);

    const lpHz = Math.max(1200, Math.min(22000, s.lpHz ?? 11000));
    for (const l of [lp1, lp2]) {
      l.frequency.cancelScheduledValues(time);
      l.frequency.setValueAtTime(l.frequency.value, time);
      l.frequency.linearRampToValueAtTime(lpHz, t1);
    }

    bump.frequency.cancelScheduledValues(time);
    bump.frequency.setValueAtTime(bump.frequency.value, time);
    bump.frequency.linearRampToValueAtTime(Math.max(30, Math.min(220, s.headBumpHz ?? 85)), t1);
    bump.gain.cancelScheduledValues(time);
    bump.gain.setValueAtTime(bump.gain.value, time);
    bump.gain.linearRampToValueAtTime(Math.max(0, Math.min(12, s.headBumpDb ?? 2.2)), t1);

    processor.parameters.get("speed")?.setValueAtTime(s.speed ?? 1, time);
    processor.parameters.get("wowDepthMs")?.setValueAtTime(s.wowDepthMs ?? 3.5, time);
    processor.parameters.get("flutterDepthMs")?.setValueAtTime(s.flutterDepthMs ?? 1.2, time);
    processor.parameters.get("wowAmount")?.setValueAtTime(s.wow ?? 0.25, time);
    processor.parameters.get("drive")?.setValueAtTime(s.drive ?? 0.35, time);
    processor.parameters.get("comp")?.setValueAtTime(s.comp ?? 0.28, time);
    processor.parameters.get("hiss")?.setValueAtTime(s.hiss ?? 0.12, time);
    processor.parameters.get("hum")?.setValueAtTime(s.hum ?? 0.05, time);
    processor.parameters.get("dropout")?.setValueAtTime(s.dropout ?? 0.18, time);
    processor.parameters.get("dropoutMs")?.setValueAtTime(s.dropoutMs ?? 38, time);
    processor.parameters.get("ceiling")?.setValueAtTime(s.ceiling ?? 0.92, time);
    processor.parameters.get("outGain")?.setValueAtTime(s.outGain ?? 0.98, time);
  }

  return {
    input,
    sfx: sfxGain,
    output: out,
    // The variable delay is centred on 12 ms. Hosts that blend this graph with
    // a dry path must delay the dry signal by the same amount or the blend
    // becomes a severe fixed comb filter.
    latencySeconds: TAPE_LATENCY_SECONDS,
    mixLaw: "linear",
    nodes: { input, sfxGain, mix, processor, hp, bump, lp1, lp2, out },
    reset,
    applySettings,
  };
}
