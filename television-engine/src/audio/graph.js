const WORKLET_URL = new URL("./tv-noise-processor.js", import.meta.url);

function now(ctx) {
  return ctx.currentTime;
}

function clamp01(x) {
  return Math.min(1, Math.max(0, x));
}

function eqPowGains(wet01) {
  const w = Math.min(1, Math.max(0, wet01));
  const a = w * Math.PI * 0.5;
  return { dry: Math.cos(a), wet: Math.sin(a) };
}

function makeSoftClipCurve(k = 2.4, n = 2048) {
  const curve = new Float32Array(n);
  const dk = Math.max(0.1, k);
  const norm = Math.tanh(dk) || 1;
  for (let i = 0; i < n; i++) {
    const x = (i / (n - 1)) * 2 - 1;
    curve[i] = Math.tanh(dk * x) / norm;
  }
  return curve;
}

export async function ensureWorklet(ctx) {
  if (ctx.__tvNoiseLoaded) return;
  await ctx.audioWorklet.addModule(WORKLET_URL.href);
  ctx.__tvNoiseLoaded = true;
}

export function defaultSettings() {
  return {
    vibe: 0.45,
    speaker: 0.55,
    agc: 0.22,
    static: 0.12,
    hum: 0.18,
    whine: 0.08,

    hpHz: 70,
    lpHz: 9000,
    midHumpDb: 1.2,
    midFreq: 1800,
    noiseHiss: 0.55,
    noiseCrackle: 0.08,

    bedEnable: true,
    bedLevel: 0.5,
    bedSource: "crt.mp3",

    outGain: 1,
  };
}

function computeMacroTargets(s) {
  const vibe = clamp01(s.vibe ?? 0.45);
  const speaker = clamp01(s.speaker ?? 0.55);
  const agc = clamp01(s.agc ?? 0.22);
  const staticAmt = clamp01(s.static ?? 0.12);
  const hum = clamp01(s.hum ?? 0.18);
  const whine = clamp01(s.whine ?? 0.08);

  const v = Math.pow(vibe, 1.15);
  const sp = Math.pow(speaker, 1.15);
  const a = Math.pow(agc, 1.25);
  const st = Math.pow(staticAmt, 1.2);

  const hpHz = Math.round(45 + (1 - sp) * 110 + v * 30);
  const lpHz = Math.round(16000 - (1 - sp) * 10000 - v * 2600);
  const midHumpDb = Math.round((0.6 + (1 - sp) * 2.4 + v * 0.6) * 20) / 20;
  const midFreq = Math.round(1550 + (1 - sp) * 650);

  const noiseHiss = clamp01(0.45 + st * 0.5);
  const noiseCrackle = clamp01(0.04 + v * 0.12);

  const compAmt = clamp01(0.08 + a * 0.65);
  const drive = 0.35 + a * 1.2;

  return {
    hpHz,
    lpHz,
    midHumpDb,
    midFreq,
    noiseHiss,
    noiseCrackle,
    compAmt,
    drive,
    hum,
    whine,
  };
}

export async function buildTelevisionGraph(ctx, { seed } = {}) {
  await ensureWorklet(ctx);
  const input = new GainNode(ctx, { gain: 1 });

  const hp1 = new BiquadFilterNode(ctx, { type: "highpass", Q: 0.707, frequency: 70 });
  const hp2 = new BiquadFilterNode(ctx, { type: "highpass", Q: 0.707, frequency: 70 });
  const dip = new BiquadFilterNode(ctx, { type: "peaking", frequency: 650, Q: 0.9, gain: -0.6 });
  const hump = new BiquadFilterNode(ctx, { type: "peaking", frequency: 1800, Q: 1.15, gain: 1.2 });
  const lp1 = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.85, frequency: 9000 });
  const lp2 = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.85, frequency: 9000 });

  const shaper = new WaveShaperNode(ctx, { curve: makeSoftClipCurve(2.4), oversample: "4x" });
  const comp = new DynamicsCompressorNode(ctx, {
    threshold: -18,
    knee: 18,
    ratio: 3,
    attack: 0.012,
    release: 0.14,
  });

  const sfx = new GainNode(ctx, { gain: 1 });
  const sfxSum = new GainNode(ctx, { gain: 1 });

  const humOsc = new OscillatorNode(ctx, { type: "sine", frequency: 60 });
  const humGain = new GainNode(ctx, { gain: 0 });
  const humLP = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.707, frequency: 280 });
  humOsc.connect(humGain);
  humGain.connect(humLP);
  humLP.connect(sfxSum);
  humOsc.start();

  const whineHz = Math.min(15734, Math.max(2000, (ctx.sampleRate || 48000) * 0.45));
  const whineOsc = new OscillatorNode(ctx, { type: "sine", frequency: whineHz });
  const whineGain = new GainNode(ctx, { gain: 0 });
  whineOsc.connect(whineGain);
  whineGain.connect(sfxSum);
  whineOsc.start();

  // A real NTSC flyback tone sits near 15.7 kHz and disappears for many
  // listeners/speakers. Its octave-down component only emerges at stronger
  // settings, preserving realism at the low end while making exaggeration useful.
  const flybackSubOsc = new OscillatorNode(ctx, { type: "sine", frequency: whineHz * 0.5 });
  const flybackSubGain = new GainNode(ctx, { gain: 0 });
  flybackSubOsc.connect(flybackSubGain);
  flybackSubGain.connect(sfxSum);
  flybackSubOsc.start();

  const noise = new AudioWorkletNode(ctx, "tv-noise", {
    numberOfInputs: 0,
    numberOfOutputs: 1,
    outputChannelCount: [1],
    processorOptions: { seed: (seed ?? 0x12345678) >>> 0 },
  });
  const noiseGain = new GainNode(ctx, { gain: 0 });
  const noiseHP = new BiquadFilterNode(ctx, { type: "highpass", Q: 0.707, frequency: 1800 });
  noise.connect(noiseGain);
  noiseGain.connect(noiseHP);
  noiseHP.connect(sfxSum);

  // External bed sources can connect to `sfx`.
  sfx.connect(sfxSum);

  const sfxLevel = new GainNode(ctx, { gain: 1 });
  sfxSum.connect(sfxLevel);

  const sum = new GainNode(ctx, { gain: 1 });
  const outGain = new GainNode(ctx, { gain: 1 });

  input.connect(hp1);
  hp1.connect(hp2);
  hp2.connect(dip);
  dip.connect(hump);
  hump.connect(lp1);
  lp1.connect(lp2);
  lp2.connect(shaper);
  shaper.connect(comp);
  comp.connect(sum);

  sfxLevel.connect(sum);
  sum.connect(outGain);

  function reset() {
    // no-op (deterministic oscillators + seeded noise)
  }

  function applySettings(settings, { time = now(ctx), ramp = 0.02 } = {}) {
    const t1 = time + ramp;
    const s = { ...defaultSettings(), ...(settings || {}) };
    const macro = computeMacroTargets(s);

    const hpHz = Math.max(20, Math.min(1200, Number(s.hpHz ?? macro.hpHz)));
    const lpHz = Math.max(800, Math.min(18000, Number(s.lpHz ?? macro.lpHz)));
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

    const midDb = Math.max(-6, Math.min(10, Number(s.midHumpDb ?? macro.midHumpDb)));
    const midF = Math.max(600, Math.min(5000, Number(s.midFreq ?? macro.midFreq)));
    hump.frequency.setValueAtTime(midF, time);
    hump.gain.setValueAtTime(midDb, time);
    dip.gain.setValueAtTime(-0.35 * midDb, time);

    const drive = Math.max(0.2, Math.min(2.4, Number(macro.drive)));
    shaper.curve = makeSoftClipCurve(2.2 + drive * 0.55);

    const compAmt = clamp01(macro.compAmt);
    comp.threshold.setValueAtTime(-10 - compAmt * 22, time);
    comp.ratio.setValueAtTime(2 + compAmt * 8, time);
    comp.knee.setValueAtTime(14 + compAmt * 20, time);
    comp.attack.setValueAtTime(0.03 - compAmt * 0.022, time);
    comp.release.setValueAtTime(0.18 - compAmt * 0.08, time);

    const staticAmt = clamp01(s.static ?? 0.12);
    noise.parameters.get("level")?.setValueAtTime(Math.pow(staticAmt, 0.72), time);
    noise.parameters.get("hiss")?.setValueAtTime(clamp01(s.noiseHiss ?? macro.noiseHiss), time);
    noise.parameters.get("crackle")?.setValueAtTime(clamp01(s.noiseCrackle ?? macro.noiseCrackle), time);
    noise.parameters.get("seed")?.setValueAtTime(((seed ?? 1) ^ 0x6d2b79f5) >>> 0, time);

    noiseGain.gain.cancelScheduledValues(time);
    noiseGain.gain.setValueAtTime(noiseGain.gain.value, time);
    noiseGain.gain.linearRampToValueAtTime(1.15, t1);
    noiseHP.frequency.setValueAtTime(1200 + macro.noiseHiss * 5200, time);

    const humAmt = clamp01(s.hum ?? macro.hum);
    humGain.gain.cancelScheduledValues(time);
    humGain.gain.setValueAtTime(humGain.gain.value, time);
    humGain.gain.linearRampToValueAtTime(humAmt * 0.045, t1);

    const wh = clamp01(s.whine ?? macro.whine);
    whineGain.gain.cancelScheduledValues(time);
    whineGain.gain.setValueAtTime(whineGain.gain.value, time);
    whineGain.gain.linearRampToValueAtTime(wh * 0.004, t1);
    flybackSubGain.gain.cancelScheduledValues(time);
    flybackSubGain.gain.setValueAtTime(flybackSubGain.gain.value, time);
    flybackSubGain.gain.linearRampToValueAtTime(wh * wh * 0.006, t1);

    const bedLevel = clamp01(s.bedLevel ?? 0.5);
    sfxLevel.gain.cancelScheduledValues(time);
    sfxLevel.gain.setValueAtTime(sfxLevel.gain.value, time);
    sfxLevel.gain.linearRampToValueAtTime(bedLevel, t1);

    const og = Math.max(0, Math.min(1.5, Number(s.outGain ?? 1)));
    outGain.gain.cancelScheduledValues(time);
    outGain.gain.setValueAtTime(outGain.gain.value, time);
    outGain.gain.linearRampToValueAtTime(og, t1);
  }

  return {
    input,
    output: outGain,
    nodes: { input, hp1, hp2, dip, hump, lp1, lp2, shaper, comp, sfx, sfxSum, sfxLevel, humOsc, humGain, whineOsc, whineGain, flybackSubOsc, flybackSubGain, noise, noiseGain, noiseHP, sum, outGain },
    reset,
    applySettings,
  };
}
