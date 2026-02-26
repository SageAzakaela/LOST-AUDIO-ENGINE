const WORKLET_URL = new URL("./cd-processor.js", import.meta.url);

function now(ctx) {
  return ctx.currentTime;
}

export async function ensureWorklet(ctx) {
  if (ctx.__cdWorkletLoaded) return;
  await ctx.audioWorklet.addModule(WORKLET_URL.href);
  ctx.__cdWorkletLoaded = true;
}

export function defaultSettings() {
  return {
    clarity: 0.65,
    damage: 0.25,
    tracking: 0.22,
    jitter: 0.18,
    carComp: 0,
    softClip: true,

    mode: "hold",
    errorRate: 0.18,
    burstMs: 24,
    repeatMs: 42,
    scratchRate: 0.25,
    scratchAmt: 0.35,
    jitterMs: 0.18,
    jitterRate: 38,
    hfLoss: 0.1,
    servoNoise: 0.12,
    ceiling: 0.94,
    outGain: 0.98,
  };
}

function modeToIndex(mode) {
  if (typeof mode === "number") return Math.round(Math.max(0, Math.min(3, mode)));
  if (mode === "mute") return 1;
  if (mode === "interp") return 2;
  if (mode === "repeat") return 3;
  return 0;
}

export async function buildCdGraph(ctx, { seed }) {
  await ensureWorklet(ctx);
  const input = new GainNode(ctx, { gain: 1 });

  const processor = new AudioWorkletNode(ctx, "cd", {
    numberOfInputs: 1,
    numberOfOutputs: 1,
    outputChannelCount: [1],
    processorOptions: { seed: seed >>> 0 },
  });

  // Car stereo-style multiband compression (parallel wet path).
  const dryGain = new GainNode(ctx, { gain: 1 });
  const wetGain = new GainNode(ctx, { gain: 0 });
  const wetSum = new GainNode(ctx, { gain: 1 });

  const lowX = 220;
  const highX = 3200;
  const makeLR2 = (type, freq) => [
    new BiquadFilterNode(ctx, { type, Q: 0.707, frequency: freq }),
    new BiquadFilterNode(ctx, { type, Q: 0.707, frequency: freq }),
  ];
  const lowLp = makeLR2("lowpass", lowX);
  const midHp = makeLR2("highpass", lowX);
  const midLp = makeLR2("lowpass", highX);
  const highHp = makeLR2("highpass", highX);

  const compLow = new DynamicsCompressorNode(ctx, { threshold: -24, knee: 18, ratio: 4, attack: 0.01, release: 0.14 });
  const compMid = new DynamicsCompressorNode(ctx, { threshold: -22, knee: 18, ratio: 4, attack: 0.008, release: 0.12 });
  const compHigh = new DynamicsCompressorNode(ctx, { threshold: -28, knee: 18, ratio: 4, attack: 0.004, release: 0.1 });
  const makeupLow = new GainNode(ctx, { gain: 1 });
  const makeupMid = new GainNode(ctx, { gain: 1 });
  const makeupHigh = new GainNode(ctx, { gain: 1 });

  const limiter = new DynamicsCompressorNode(ctx, {
    threshold: -1.2,
    knee: 0,
    ratio: 20,
    attack: 0.003,
    release: 0.08,
  });

  const out = new GainNode(ctx, { gain: 1 });
  input.connect(processor);
  processor.connect(dryGain);

  // Low band
  processor.connect(lowLp[0]);
  lowLp[0].connect(lowLp[1]);
  lowLp[1].connect(compLow);
  compLow.connect(makeupLow);
  makeupLow.connect(wetSum);

  // Mid band
  processor.connect(midHp[0]);
  midHp[0].connect(midHp[1]);
  midHp[1].connect(midLp[0]);
  midLp[0].connect(midLp[1]);
  midLp[1].connect(compMid);
  compMid.connect(makeupMid);
  makeupMid.connect(wetSum);

  // High band
  processor.connect(highHp[0]);
  highHp[0].connect(highHp[1]);
  highHp[1].connect(compHigh);
  compHigh.connect(makeupHigh);
  makeupHigh.connect(wetSum);

  wetSum.connect(wetGain);

  const sum = new GainNode(ctx, { gain: 1 });
  dryGain.connect(sum);
  wetGain.connect(sum);
  sum.connect(limiter);
  limiter.connect(out);

  function reset(seedNext) {
    processor.port.postMessage({ type: "reset", seed: seedNext >>> 0 });
  }

  function applySettings(settings, { time = now(ctx), ramp = 0.02 } = {}) {
    const s = { ...settings };
    const mode = modeToIndex(s.mode);
    processor.parameters.get("mode")?.setValueAtTime(mode, time);
    processor.parameters.get("errorRate")?.setValueAtTime(s.errorRate ?? 0.18, time);
    processor.parameters.get("burstMs")?.setValueAtTime(s.burstMs ?? 24, time);
    processor.parameters.get("repeatMs")?.setValueAtTime(s.repeatMs ?? 42, time);
    processor.parameters.get("scratchRate")?.setValueAtTime(s.scratchRate ?? 0.25, time);
    processor.parameters.get("scratchAmt")?.setValueAtTime(s.scratchAmt ?? 0.35, time);
    processor.parameters.get("jitterMs")?.setValueAtTime(s.jitterMs ?? 0.18, time);
    processor.parameters.get("jitterRate")?.setValueAtTime(s.jitterRate ?? 38, time);
    processor.parameters.get("hfLoss")?.setValueAtTime(s.hfLoss ?? 0.1, time);
    processor.parameters.get("servoNoise")?.setValueAtTime(s.servoNoise ?? 0.12, time);
    processor.parameters.get("softClip")?.setValueAtTime(s.softClip ? 1 : 0, time);
    processor.parameters.get("ceiling")?.setValueAtTime(s.ceiling ?? 0.94, time);
    processor.parameters.get("outGain")?.setValueAtTime(s.outGain ?? 0.98, time);

    // Ramp a couple of the most audible knobs for smoothness.
    const t1 = time + ramp;
    processor.parameters.get("outGain")?.linearRampToValueAtTime(s.outGain ?? 0.98, t1);
    processor.parameters.get("ceiling")?.linearRampToValueAtTime(s.ceiling ?? 0.94, t1);

    // Multiband comp: wet/dry mix + threshold/ratio scaling.
    const amt = Math.max(0, Math.min(1, s.carComp ?? 0));
    dryGain.gain.cancelScheduledValues(time);
    dryGain.gain.setValueAtTime(dryGain.gain.value, time);
    dryGain.gain.linearRampToValueAtTime(1 - amt, t1);
    wetGain.gain.cancelScheduledValues(time);
    wetGain.gain.setValueAtTime(wetGain.gain.value, time);
    wetGain.gain.linearRampToValueAtTime(amt, t1);

    const thrLow = -14 - amt * 24;
    const thrMid = -12 - amt * 26;
    const thrHigh = -16 - amt * 28;
    const ratio = 1.2 + amt * 6.5;
    const knee = 6 + amt * 26;

    for (const [c, thr] of [
      [compLow, thrLow],
      [compMid, thrMid],
      [compHigh, thrHigh],
    ]) {
      c.threshold.setValueAtTime(thr, time);
      c.ratio.setValueAtTime(ratio, time);
      c.knee.setValueAtTime(knee, time);
    }

    const makeup = 1 + amt * 0.35;
    makeupLow.gain.setValueAtTime(makeup * (1 + amt * 0.12), time);
    makeupMid.gain.setValueAtTime(makeup, time);
    makeupHigh.gain.setValueAtTime(makeup * (0.92 + amt * 0.08), time);
  }

  return {
    input,
    output: out,
    nodes: {
      input,
      processor,
      dryGain,
      wetGain,
      wetSum,
      compLow,
      compMid,
      compHigh,
      makeupLow,
      makeupMid,
      makeupHigh,
      limiter,
      out,
    },
    reset,
    applySettings,
  };
}
