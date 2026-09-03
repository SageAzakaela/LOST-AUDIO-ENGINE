const WORKLET_URL = new URL("./camcorder-processor.js?v=20260828.28", import.meta.url);

function now(ctx) {
  return ctx.currentTime;
}

export async function ensureWorklet(ctx) {
  if (ctx.__camcorderWorkletLoaded) return;
  await ctx.audioWorklet.addModule(WORKLET_URL.href);
  ctx.__camcorderWorkletLoaded = true;
}

export function defaultSettings() {
  return {
    coverage: 0.35,
    movement: 0.25,
    corruption: 0.18,
    agc: 0.35,
    wind: false,
    windLevel: 0.95,
    camLevel: 0.35,
    windBedLevel: 0.85,
    windHitLevel: 0.65,
    windHitRate: 0.35,
    camBedSource: "",
    windBedSource: "",
    windHitSource: "",
    format: "minidv",
    micModel: "electret",

    hpHz: 55,
    lpHz: 9200,
    boxDb: 3.2,
    boxHz: 1650,

    agcAmt: 0.55,
    agcSpeed: 0.45,
    agcPump: 0.45,
    clip: 0.25,

    crush: 0.12,
    bits: 12,
    rate: 24000,
    flutter: 0.12,

    drop: 0.18,
    dropMs: 28,
    dropMode: "hold",
    repeatMs: 48,
    chirp: 0.15,

    handling: 0.22,
    rub: 0.18,
    hiss: 0.12,
    motorBleed: 0.08,

    ceiling: 0.92,
    outGain: 0.98,
  };
}

function formatToIndex(format) {
  if (typeof format === "number") return Math.round(Math.max(0, Math.min(4, format)));
  return { vhsc: 0, video8: 1, minidv: 2, digicam: 3, action: 4 }[format] ?? 2;
}

function micModelToIndex(model) {
  if (typeof model === "number") return Math.round(Math.max(0, Math.min(4, model)));
  return { electret: 0, cheapMono: 1, stereo: 2, waterproof: 3, shotgun: 4 }[model] ?? 0;
}

function dropModeToIndex(mode) {
  if (typeof mode === "number") return Math.round(Math.max(0, Math.min(3, mode)));
  if (mode === "mute") return 1;
  if (mode === "interp") return 2;
  if (mode === "repeat") return 3;
  return 0;
}

export async function buildCamcorderGraph(ctx, { seed }) {
  await ensureWorklet(ctx);
  const input = new GainNode(ctx, { gain: 1 });
  const wind = new GainNode(ctx, { gain: 0 });
  const camera = new GainNode(ctx, { gain: 0.1 });

  const hp = new BiquadFilterNode(ctx, { type: "highpass", Q: 0.707, frequency: 55 });
  const lp1 = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.85, frequency: 9200 });
  const lp2 = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.85, frequency: 9200 });
  const box = new BiquadFilterNode(ctx, { type: "peaking", Q: 1.2, frequency: 1650, gain: 3.2 });
  const dip = new BiquadFilterNode(ctx, { type: "peaking", Q: 0.9, frequency: 650, gain: -1.2 });

  const processor = new AudioWorkletNode(ctx, "camcorder", {
    numberOfInputs: 1,
    numberOfOutputs: 1,
    outputChannelCount: [1],
    processorOptions: { seed: seed >>> 0 },
  });

  // Imported beds are environmental production elements. Keep them out of
  // converter/dropout corruption, calibrate their mastered-at-0-dB level, and
  // protect the final sum independently.
  const windHp = new BiquadFilterNode(ctx, { type: "highpass", Q: 0.707, frequency: 45 });
  const windLp = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.707, frequency: 5200 });
  const windComp = new DynamicsCompressorNode(ctx, { threshold: -24, knee: 18, ratio: 4, attack: 0.008, release: 0.18 });
  const postSum = new GainNode(ctx, { gain: 1 });
  const safetyCurve = new Float32Array(4097);
  for (let i = 0; i < safetyCurve.length; i++) {
    const x = (i / (safetyCurve.length - 1)) * 2 - 1;
    safetyCurve[i] = Math.max(-0.98, Math.min(0.98, x));
  }
  const safetyPre = new GainNode(ctx, { gain: 0.98 / 0.92 });
  const safetyClip = new WaveShaperNode(ctx, { curve: safetyCurve, oversample: "none" });
  const safetyPost = new GainNode(ctx, { gain: 0.92 / 0.98 });
  const out = new GainNode(ctx, { gain: 1 });

  input.connect(hp);
  hp.connect(box);
  box.connect(dip);
  dip.connect(lp1);
  lp1.connect(lp2);
  lp2.connect(processor);
  processor.connect(postSum);
  wind.connect(windHp);
  camera.connect(windHp);
  windHp.connect(windLp);
  windLp.connect(windComp);
  windComp.connect(postSum);
  postSum.connect(safetyPre);
  safetyPre.connect(safetyClip);
  safetyClip.connect(safetyPost);
  safetyPost.connect(out);

  function reset(seedNext) {
    processor.port.postMessage({ type: "reset", seed: seedNext >>> 0 });
  }

  function applySettings(settings, { time = now(ctx), ramp = 0.02 } = {}) {
    const t1 = time + ramp;
    const s = { ...settings };

    hp.frequency.cancelScheduledValues(time);
    hp.frequency.setValueAtTime(hp.frequency.value, time);
    hp.frequency.linearRampToValueAtTime(Math.max(10, Math.min(280, s.hpHz ?? 55)), t1);

    const lpHz = Math.max(900, Math.min(22000, s.lpHz ?? 9200));
    for (const l of [lp1, lp2]) {
      l.frequency.cancelScheduledValues(time);
      l.frequency.setValueAtTime(l.frequency.value, time);
      l.frequency.linearRampToValueAtTime(lpHz, t1);
    }

    box.frequency.cancelScheduledValues(time);
    box.frequency.setValueAtTime(box.frequency.value, time);
    box.frequency.linearRampToValueAtTime(Math.max(650, Math.min(4200, s.boxHz ?? 1650)), t1);
    box.gain.cancelScheduledValues(time);
    box.gain.setValueAtTime(box.gain.value, time);
    box.gain.linearRampToValueAtTime(Math.max(0, Math.min(14, s.boxDb ?? 3.2)), t1);
    dip.gain.cancelScheduledValues(time);
    dip.gain.setValueAtTime(dip.gain.value, time);
    dip.gain.linearRampToValueAtTime(-0.35 * Math.max(0, Math.min(14, s.boxDb ?? 3.2)), t1);

    processor.parameters.get("coverage")?.setValueAtTime(s.coverage ?? 0.35, time);
    processor.parameters.get("movement")?.setValueAtTime(s.movement ?? 0.25, time);
    processor.parameters.get("corruption")?.setValueAtTime(s.corruption ?? 0.18, time);
    processor.parameters.get("agcDrive")?.setValueAtTime(s.agc ?? 0.35, time);
    processor.parameters.get("wind")?.setValueAtTime(s.wind ? 1 : 0, time);
    processor.parameters.get("windLevel")?.setValueAtTime(s.windLevel ?? 0.95, time);
    processor.parameters.get("format")?.setValueAtTime(formatToIndex(s.format), time);
    processor.parameters.get("micModel")?.setValueAtTime(micModelToIndex(s.micModel), time);
    wind.gain.cancelScheduledValues(time);
    wind.gain.setValueAtTime(wind.gain.value, time);
    // The supplied wind beds average roughly -6 to -9 dBFS. A calibrated
    // -20 dB pad makes them a camera layer instead of the entire mix.
    wind.gain.linearRampToValueAtTime(s.wind ? Math.max(0, Math.min(1.5, s.windLevel ?? 0.95)) * 0.1 : 0, t1);

    processor.parameters.get("agcAmt")?.setValueAtTime(s.agcAmt ?? 0.55, time);
    processor.parameters.get("agcSpeed")?.setValueAtTime(s.agcSpeed ?? 0.45, time);
    processor.parameters.get("agcPump")?.setValueAtTime(s.agcPump ?? 0.45, time);
    processor.parameters.get("clip")?.setValueAtTime(s.clip ?? 0.25, time);

    processor.parameters.get("crush")?.setValueAtTime(s.crush ?? 0.12, time);
    processor.parameters.get("bits")?.setValueAtTime(s.bits ?? 12, time);
    processor.parameters.get("rate")?.setValueAtTime(s.rate ?? 24000, time);
    processor.parameters.get("flutter")?.setValueAtTime(s.flutter ?? 0.12, time);

    processor.parameters.get("drop")?.setValueAtTime(s.drop ?? 0.18, time);
    processor.parameters.get("dropMs")?.setValueAtTime(s.dropMs ?? 28, time);
    processor.parameters.get("dropMode")?.setValueAtTime(dropModeToIndex(s.dropMode), time);
    processor.parameters.get("repeatMs")?.setValueAtTime(s.repeatMs ?? 48, time);
    processor.parameters.get("chirp")?.setValueAtTime(s.chirp ?? 0.15, time);

    processor.parameters.get("handling")?.setValueAtTime(s.handling ?? 0.22, time);
    processor.parameters.get("rub")?.setValueAtTime(s.rub ?? 0.18, time);
    processor.parameters.get("hiss")?.setValueAtTime(s.hiss ?? 0.12, time);
    processor.parameters.get("motorBleed")?.setValueAtTime(s.motorBleed ?? 0.08, time);

    processor.parameters.get("ceiling")?.setValueAtTime(s.ceiling ?? 0.92, time);
    processor.parameters.get("outGain")?.setValueAtTime(s.outGain ?? 0.98, time);

    const finalCeiling = Math.max(0.2, Math.min(1, s.ceiling ?? 0.92));
    safetyPre.gain.cancelScheduledValues(time);
    safetyPre.gain.setValueAtTime(safetyPre.gain.value, time);
    safetyPre.gain.linearRampToValueAtTime(0.98 / finalCeiling, t1);
    safetyPost.gain.cancelScheduledValues(time);
    safetyPost.gain.setValueAtTime(safetyPost.gain.value, time);
    safetyPost.gain.linearRampToValueAtTime(finalCeiling / 0.98, t1);
  }

  return {
    input,
    wind,
    camera,
    output: out,
    nodes: { input, wind, camera, windHp, windLp, windComp, postSum, safetyPre, safetyClip, safetyPost, hp, box, dip, lp1, lp2, processor, out },
    reset,
    applySettings,
  };
}
