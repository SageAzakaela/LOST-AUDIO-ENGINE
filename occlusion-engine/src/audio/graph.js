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

function hzClamp(ctx, hz, { min = 10 } = {}) {
  const nyq = (ctx.sampleRate || 48000) * 0.5;
  const hi = Math.max(min, nyq * 0.95);
  const v = Number.isFinite(hz) ? hz : min;
  return Math.max(min, Math.min(hi, v));
}

function materialDefaults(material) {
  const m = String(material || "drywall");
  if (m === "brick") return { lpMin: 1100, dipHz: 1650, dipDb: -3.0, bumpHz: 420, bumpDb: 1.8, damp: 0.75, leakBias: -0.05 };
  if (m === "wood") return { lpMin: 1500, dipHz: 1450, dipDb: -2.2, bumpHz: 520, bumpDb: 1.6, damp: 0.65, leakBias: 0.02 };
  if (m === "curtain") return { lpMin: 2200, dipHz: 1800, dipDb: -1.2, bumpHz: 360, bumpDb: 0.9, damp: 0.55, leakBias: 0.18 };
  if (m === "door") return { lpMin: 1350, dipHz: 1550, dipDb: -2.6, bumpHz: 380, bumpDb: 1.4, damp: 0.72, leakBias: 0.08 };
  if (m === "glass") return { lpMin: 2600, dipHz: 1250, dipDb: -1.0, bumpHz: 900, bumpDb: 1.2, damp: 0.45, leakBias: 0.25 };
  return { lpMin: 1800, dipHz: 1600, dipDb: -2.0, bumpHz: 420, bumpDb: 1.2, damp: 0.68, leakBias: 0.05 }; // drywall
}

export function defaultSettings() {
  return {
    distance: 0.35,
    wall: 0.45,
    material: "drywall",
    sourceRoom: 0.35,
    listenerRoom: 0.45,

    hpHz: 50,
    lpHz: 5200,
    dipHz: 1600,
    dipDb: -2.0,
    dipQ: 1.1,
    bumpHz: 420,
    bumpDb: 1.2,
    bumpQ: 0.95,

    leak: 0.08,
    roomMix: 0.22,
    predelayMs: 12,
    roomSize: 0.5,
    damp: 0.68,
    outGain: 1,
  };
}

function computeMacroTargets(s, ctx) {
  const dist = clamp01(s.distance ?? 0.35);
  const wall = clamp01(s.wall ?? 0.45);
  const srcRoom = clamp01(s.sourceRoom ?? 0.35);
  const lisRoom = clamp01(s.listenerRoom ?? 0.45);
  const mat = materialDefaults(s.material);

  const d = Math.pow(dist, 1.15);
  const w = Math.pow(wall, 1.2);
  const room = clamp01(0.45 * srcRoom + 0.55 * lisRoom);

  const hpHz = 35 + d * 75 + w * 45;
  const lpHz = 16000 - (d * 5500 + w * 9500);
  const lpMin = mat.lpMin + w * 250;

  const dipDb = mat.dipDb - w * 1.8;
  const bumpDb = mat.bumpDb + w * 2.2;
  const dipHz = mat.dipHz + (0.5 - room) * 140;
  const bumpHz = mat.bumpHz + (0.5 - room) * 120;

  const leak = clamp01(0.03 + (1 - w) * 0.18 + mat.leakBias);
  const roomMix = clamp01(0.08 + room * 0.32 + d * 0.18);
  const predelayMs = Math.round(6 + room * 26 + d * 10);
  const damp = clamp01(mat.damp + room * 0.12);
  const roomSize = clamp01(room);

  const outGain = Math.round((1.0 - d * 0.18) * 100) / 100;

  return {
    hpHz: hzClamp(ctx, hpHz, { min: 10 }),
    lpHz: hzClamp(ctx, Math.max(lpMin, lpHz), { min: 80 }),
    dipHz: hzClamp(ctx, dipHz, { min: 120 }),
    bumpHz: hzClamp(ctx, bumpHz, { min: 120 }),
    dipDb,
    bumpDb,
    leak,
    roomMix,
    predelayMs,
    damp,
    roomSize,
    outGain,
  };
}

function makeRoomVerb(ctx) {
  const input = new GainNode(ctx, { gain: 1 });
  const predelay = new DelayNode(ctx, { delayTime: 0.012, maxDelayTime: 0.12 });
  const dry = new GainNode(ctx, { gain: 1 });
  const wet = new GainNode(ctx, { gain: 0.22 });
  const sum = new GainNode(ctx, { gain: 1 });

  const combs = [];
  const combSum = new GainNode(ctx, { gain: 1 });
  const combTimes = [0.0297, 0.0371, 0.0411, 0.0437];
  for (const t of combTimes) {
    const d = new DelayNode(ctx, { delayTime: t, maxDelayTime: 0.08 });
    const fb = new GainNode(ctx, { gain: 0.6 });
    const damp = new BiquadFilterNode(ctx, { type: "lowpass", frequency: 5200, Q: 0.707 });
    predelay.connect(d);
    d.connect(fb);
    fb.connect(damp);
    damp.connect(d);
    d.connect(combSum);
    combs.push({ d, fb, damp, base: t });
  }

  const ap1 = new DelayNode(ctx, { delayTime: 0.0083, maxDelayTime: 0.02 });
  const ap1Fb = new GainNode(ctx, { gain: 0.5 });
  const ap2 = new DelayNode(ctx, { delayTime: 0.0121, maxDelayTime: 0.03 });
  const ap2Fb = new GainNode(ctx, { gain: 0.5 });

  combSum.connect(ap1);
  ap1.connect(ap1Fb);
  ap1Fb.connect(ap1);
  ap1.connect(ap2);
  ap2.connect(ap2Fb);
  ap2Fb.connect(ap2);
  ap2.connect(wet);

  input.connect(dry);
  input.connect(predelay);
  dry.connect(sum);
  wet.connect(sum);

  function apply({ roomMix = 0.22, predelayMs = 12, roomSize = 0.5, damp = 0.68, decay = 0.6 } = {}, { time = now(ctx), ramp = 0.02 } = {}) {
    const t1 = time + ramp;
    const mix = eqPowGains(clamp01(roomMix));
    dry.gain.cancelScheduledValues(time);
    dry.gain.setValueAtTime(dry.gain.value, time);
    dry.gain.linearRampToValueAtTime(mix.dry, t1);
    wet.gain.cancelScheduledValues(time);
    wet.gain.setValueAtTime(wet.gain.value, time);
    wet.gain.linearRampToValueAtTime(mix.wet, t1);

    const pd = Math.max(0, Math.min(0.12, (Number(predelayMs) || 0) / 1000));
    predelay.delayTime.cancelScheduledValues(time);
    predelay.delayTime.setValueAtTime(predelay.delayTime.value, time);
    predelay.delayTime.linearRampToValueAtTime(pd, t1);

    const size = clamp01(roomSize);
    for (const c of combs) {
      const dt = Math.max(0.002, Math.min(0.08, c.base * (0.85 + size * 0.45)));
      c.d.delayTime.setValueAtTime(dt, time);
      const fb = Math.max(0, Math.min(0.92, decay));
      c.fb.gain.setValueAtTime(fb, time);
      const dampHz = 1200 + clamp01(damp) * 9000;
      c.damp.frequency.setValueAtTime(hzClamp(ctx, dampHz, { min: 200 }), time);
    }
  }

  return { input, output: sum, apply, nodes: { input, predelay, dry, wet, sum, combSum, combs, ap1, ap1Fb, ap2, ap2Fb } };
}

export async function buildOcclusionGraph(ctx, { seed } = {}) {
  void seed;
  const input = new GainNode(ctx, { gain: 1 });

  const hp1 = new BiquadFilterNode(ctx, { type: "highpass", frequency: 50, Q: 0.707 });
  const hp2 = new BiquadFilterNode(ctx, { type: "highpass", frequency: 50, Q: 0.707 });
  const bump = new BiquadFilterNode(ctx, { type: "peaking", frequency: 420, Q: 0.95, gain: 1.2 });
  const dip = new BiquadFilterNode(ctx, { type: "peaking", frequency: 1600, Q: 1.1, gain: -2.0 });
  const lp1 = new BiquadFilterNode(ctx, { type: "lowpass", frequency: 5200, Q: 0.85 });
  const lp2 = new BiquadFilterNode(ctx, { type: "lowpass", frequency: 5200, Q: 0.85 });

  const filteredGain = new GainNode(ctx, { gain: 1 });
  const leakGain = new GainNode(ctx, { gain: 0.08 });

  const room = makeRoomVerb(ctx);
  const outGain = new GainNode(ctx, { gain: 1 });
  const sum = new GainNode(ctx, { gain: 1 });

  input.connect(leakGain);
  leakGain.connect(sum);

  input.connect(hp1);
  hp1.connect(hp2);
  hp2.connect(bump);
  bump.connect(dip);
  dip.connect(lp1);
  lp1.connect(lp2);
  lp2.connect(filteredGain);
  filteredGain.connect(room.input);
  room.output.connect(outGain);
  outGain.connect(sum);

  function reset() {
    // no-op (deterministic)
  }

  function applySettings(settings, { time = now(ctx), ramp = 0.02 } = {}) {
    const t1 = time + ramp;
    const s = { ...defaultSettings(), ...(settings || {}) };
    const macro = computeMacroTargets(s, ctx);

    const hpHz = hzClamp(ctx, s.hpHz ?? macro.hpHz, { min: 10 });
    const lpHz = hzClamp(ctx, s.lpHz ?? macro.lpHz, { min: 80 });

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

    bump.frequency.setValueAtTime(hzClamp(ctx, s.bumpHz ?? macro.bumpHz, { min: 120 }), time);
    bump.Q.setValueAtTime(Math.max(0.2, Math.min(5, Number(s.bumpQ ?? 0.95))), time);
    bump.gain.setValueAtTime(Math.max(-12, Math.min(12, Number(s.bumpDb ?? macro.bumpDb))), time);

    dip.frequency.setValueAtTime(hzClamp(ctx, s.dipHz ?? macro.dipHz, { min: 120 }), time);
    dip.Q.setValueAtTime(Math.max(0.2, Math.min(8, Number(s.dipQ ?? 1.1))), time);
    dip.gain.setValueAtTime(Math.max(-18, Math.min(6, Number(s.dipDb ?? macro.dipDb))), time);

    const leak = clamp01(s.leak ?? macro.leak);
    leakGain.gain.cancelScheduledValues(time);
    leakGain.gain.setValueAtTime(leakGain.gain.value, time);
    leakGain.gain.linearRampToValueAtTime(leak, t1);

    const roomMix = clamp01(s.roomMix ?? macro.roomMix);
    const predelayMs = Number.isFinite(s.predelayMs) ? s.predelayMs : macro.predelayMs;
    const roomSize = clamp01(s.roomSize ?? macro.roomSize);
    const damp = clamp01(s.damp ?? macro.damp);
    const decay = 0.4 + roomSize * 0.48;
    room.apply({ roomMix, predelayMs, roomSize, damp, decay }, { time, ramp });

    const og = Math.max(0, Math.min(1.5, Number(s.outGain ?? macro.outGain)));
    outGain.gain.cancelScheduledValues(time);
    outGain.gain.setValueAtTime(outGain.gain.value, time);
    outGain.gain.linearRampToValueAtTime(og, t1);
  }

  return {
    input,
    output: sum,
    nodes: { input, hp1, hp2, bump, dip, lp1, lp2, leakGain, filteredGain, room, outGain, sum },
    reset,
    applySettings,
  };
}

