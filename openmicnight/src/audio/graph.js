function now(ctx) {
  return ctx.currentTime;
}

function clamp(x, lo, hi) {
  return Math.min(hi, Math.max(lo, x));
}

function eqPowGains(wet01) {
  const w = clamp(wet01, 0, 1);
  const a = w * Math.PI * 0.5;
  return { dry: Math.cos(a), wet: Math.sin(a) };
}

function makeSoftClipCurve(k = 2.8, n = 2048) {
  const curve = new Float32Array(n);
  const dk = Math.max(0.1, k);
  const norm = Math.tanh(dk) || 1;
  for (let i = 0; i < n; i++) {
    const x = (i / (n - 1)) * 2 - 1;
    curve[i] = Math.tanh(dk * x) / norm;
  }
  return curve;
}

function makeRoomIR(ctx, ms, seed) {
  const sr = ctx.sampleRate;
  const len = Math.max(64, Math.floor((clamp(ms, 30, 2500) / 1000) * sr));
  const buf = ctx.createBuffer(1, len, sr);
  const ch = buf.getChannelData(0);

  let state = (seed >>> 0) || 0x12345678;
  const rnd = () => {
    let x = state >>> 0;
    x ^= x << 13;
    x ^= x >>> 17;
    x ^= x << 5;
    state = x >>> 0;
    return (state >>> 0) / 0xffffffff;
  };

  const cut = 7000;
  const a = Math.exp((-2 * Math.PI * cut) / sr);
  let lp = 0;
  let peak = 1e-6;
  for (let i = 0; i < len; i++) {
    const t = i / (len - 1);
    const env = Math.pow(1 - t, 2.3);
    const x = (rnd() * 2 - 1) * env;
    lp = (1 - a) * x + a * lp;
    ch[i] = lp;
    peak = Math.max(peak, Math.abs(lp));
  }
  const g = 0.8 / peak;
  for (let i = 0; i < len; i++) ch[i] *= g;
  return buf;
}

export function defaultSettings() {
  return {
    hotMic: 0.55,
    fbFreq: 1800,
    ringQ: 16,
    fbDelayMs: 24,
    fbTone: 0.55,
    wall: 0.65,
    room: 0.55,
    crowdLevel: 0.25,
    outGain: 0.95,
    limit: 0.65,
  };
}

export async function buildOpenMicGraph(ctx, { seed = 0xfeedc0de } = {}) {
  const input = new GainNode(ctx, { gain: 1 });
  const crowd = new GainNode(ctx, { gain: 0 });
  const sumIn = new GainNode(ctx, { gain: 1 });

  input.connect(sumIn);
  crowd.connect(sumIn);

  // "Outside / behind wall" filters.
  const hp = new BiquadFilterNode(ctx, { type: "highpass", Q: 0.707, frequency: 55 });
  const wallLp1 = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.85, frequency: 2200 });
  const wallLp2 = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.85, frequency: 2200 });
  const wallDip = new BiquadFilterNode(ctx, { type: "peaking", Q: 0.8, frequency: 480, gain: -2.0 });

  // Feedback loop (delay -> bandpass -> saturation -> gain -> delay)
  const fbIn = new GainNode(ctx, { gain: 1 });
  const fbDelay = new DelayNode(ctx, { maxDelayTime: 0.2, delayTime: 0.024 });
  const fbBand = new BiquadFilterNode(ctx, { type: "bandpass", Q: 16, frequency: 1800 });
  const fbToneLP = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.707, frequency: 5200 });
  const fbShaper = new WaveShaperNode(ctx, { curve: makeSoftClipCurve(3.2), oversample: "4x" });
  const fbGain = new GainNode(ctx, { gain: 0.0 });

  // Summing point: program audio passes through fbIn, and the feedback loop returns into fbIn.
  sumIn.connect(fbIn);
  fbIn.connect(fbDelay);
  fbDelay.connect(fbBand);
  fbBand.connect(fbToneLP);
  fbToneLP.connect(fbShaper);
  fbShaper.connect(fbGain);
  fbGain.connect(fbDelay);

  // Feed the "hot mic" back into the main path (so it rings audibly).
  const fbAudible = new GainNode(ctx, { gain: 1 });
  fbShaper.connect(fbAudible);

  // Wall path
  const wallInputSum = new GainNode(ctx, { gain: 1 });
  sumIn.connect(wallInputSum);
  fbAudible.connect(wallInputSum);
  wallInputSum.connect(hp);
  hp.connect(wallDip);
  wallDip.connect(wallLp1);
  wallLp1.connect(wallLp2);

  // Reverb (parallel)
  const convolver = new ConvolverNode(ctx, { disableNormalization: false });
  const verbDamp = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.707, frequency: 6000 });
  const verbWet = new GainNode(ctx, { gain: 0.0 });

  // Dry
  const dryGain = new GainNode(ctx, { gain: 1 });
  const wetSum = new GainNode(ctx, { gain: 1 });

  wallLp2.connect(dryGain);
  wallLp2.connect(convolver);
  convolver.connect(verbDamp);
  verbDamp.connect(verbWet);
  dryGain.connect(wetSum);
  verbWet.connect(wetSum);

  // Limiter / output
  const limiter = new DynamicsCompressorNode(ctx, { threshold: -1.2, knee: 0, ratio: 20, attack: 0.003, release: 0.09 });
  const limiterDry = new GainNode(ctx, { gain: 1 });
  const limiterWet = new GainNode(ctx, { gain: 0 });
  const limiterSum = new GainNode(ctx, { gain: 1 });
  const outGain = new GainNode(ctx, { gain: 0.95 });

  wetSum.connect(limiterDry);
  limiterDry.connect(limiterSum);
  wetSum.connect(limiter);
  limiter.connect(limiterWet);
  limiterWet.connect(limiterSum);
  limiterSum.connect(outGain);

  // IR is deterministic per seed + room time
  let currentIrMs = null;
  function ensureIrFor(ms) {
    const next = Math.round(clamp(ms, 40, 1200));
    if (currentIrMs === next && convolver.buffer) return;
    currentIrMs = next;
    convolver.buffer = makeRoomIR(ctx, next, ((seed >>> 0) ^ 0x6a09e667 ^ (next * 2654435761)) >>> 0);
  }

  function reset(seedNext) {
    seed = seedNext >>> 0;
    currentIrMs = null;
  }

  function applySettings(settings, { time = now(ctx), ramp = 0.02 } = {}) {
    const t1 = time + ramp;
    const s = { ...settings };

    const wall = clamp(s.wall ?? 0.65, 0, 1);
    const wallLp = 9000 - wall * 7200; // 9k -> 1.8k
    for (const lp of [wallLp1, wallLp2]) {
      lp.frequency.cancelScheduledValues(time);
      lp.frequency.setValueAtTime(lp.frequency.value, time);
      lp.frequency.linearRampToValueAtTime(wallLp, t1);
    }
    wallDip.gain.setValueAtTime(-1.2 - wall * 3.4, time);
    hp.frequency.setValueAtTime(45 + wall * 80, time);

    const room = clamp(s.room ?? 0.55, 0, 1);
    ensureIrFor(120 + room * 780);
    const verbMix = clamp(0.02 + room * 0.75, 0, 1);
    const mix = eqPowGains(verbMix);
    dryGain.gain.cancelScheduledValues(time);
    dryGain.gain.setValueAtTime(dryGain.gain.value, time);
    dryGain.gain.linearRampToValueAtTime(mix.dry, t1);
    verbWet.gain.cancelScheduledValues(time);
    verbWet.gain.setValueAtTime(verbWet.gain.value, time);
    verbWet.gain.linearRampToValueAtTime(mix.wet, t1);
    const dampHz = 2600 + (1 - room) * 9000;
    verbDamp.frequency.setValueAtTime(dampHz, time);

    const hot = clamp(s.hotMic ?? 0.55, 0, 1);
    const fb = clamp(hot * hot * 0.93, 0, 0.93);
    fbGain.gain.cancelScheduledValues(time);
    fbGain.gain.setValueAtTime(fbGain.gain.value, time);
    fbGain.gain.linearRampToValueAtTime(fb, t1);

    const freq = clamp(s.fbFreq ?? 1800, 200, 5000);
    fbBand.frequency.cancelScheduledValues(time);
    fbBand.frequency.setValueAtTime(fbBand.frequency.value, time);
    fbBand.frequency.linearRampToValueAtTime(freq, t1);
    fbBand.Q.cancelScheduledValues(time);
    fbBand.Q.setValueAtTime(fbBand.Q.value, time);
    fbBand.Q.linearRampToValueAtTime(clamp(s.ringQ ?? 16, 1, 40), t1);

    const dms = clamp(s.fbDelayMs ?? 24, 8, 70);
    fbDelay.delayTime.cancelScheduledValues(time);
    fbDelay.delayTime.setValueAtTime(fbDelay.delayTime.value, time);
    fbDelay.delayTime.linearRampToValueAtTime(dms / 1000, t1);

    const tone = clamp(s.fbTone ?? 0.55, 0, 1);
    fbToneLP.frequency.setValueAtTime(1200 + tone * 7800, time);

    crowd.gain.cancelScheduledValues(time);
    crowd.gain.setValueAtTime(crowd.gain.value, time);
    crowd.gain.linearRampToValueAtTime(clamp(s.crowdLevel ?? 0.25, 0, 1), t1);

    outGain.gain.cancelScheduledValues(time);
    outGain.gain.setValueAtTime(outGain.gain.value, time);
    outGain.gain.linearRampToValueAtTime(clamp(s.outGain ?? 0.95, 0, 1.5), t1);

    const limitAmt = clamp(s.limit ?? 0.65, 0, 1);
    const limMix = eqPowGains(limitAmt);
    limiterDry.gain.setValueAtTime(limMix.dry, time);
    limiterWet.gain.setValueAtTime(limMix.wet, time);
  }

  return {
    input,
    crowd,
    output: outGain,
    nodes: { input, crowd, sumIn, fbDelay, fbBand, fbToneLP, fbShaper, fbGain, wallLp1, wallLp2, wallDip, convolver, verbWet, dryGain, limiter, outGain },
    reset,
    applySettings,
  };
}
