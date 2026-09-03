const BODY_WORKLET_URL = new URL("./obfuscation-body-processor.js?v=20260827.19", import.meta.url);

function now(ctx) {
  return ctx.currentTime;
}

async function ensureBodyWorklet(ctx) {
  if (ctx.__obfuscationBodyWorkletLoaded) return;
  await ctx.audioWorklet.addModule(BODY_WORKLET_URL.href);
  ctx.__obfuscationBodyWorkletLoaded = true;
}

function clamp01(x) {
  return Math.min(1, Math.max(0, Number(x) || 0));
}

function hzClamp(ctx, hz, { min = 10 } = {}) {
  const nyq = (ctx.sampleRate || 48000) * 0.5;
  const hi = Math.max(min, nyq * 0.95);
  const v = Number.isFinite(hz) ? hz : min;
  return Math.max(min, Math.min(hi, v));
}

function rampParam(param, value, time, ramp) {
  const target = Number.isFinite(value) ? value : 0;
  param.cancelScheduledValues(time);
  param.setValueAtTime(param.value, time);
  param.linearRampToValueAtTime(target, time + ramp);
}

function materialDefaults(material) {
  const materials = {
    drywall: {
      lpMin: 1750, dipHz: 1550, dipDb: -2.4, bumpHz: 350, bumpDb: 1.4,
      damp: 0.63, leakBias: 0.02, leakTone: 0.52, loss: 0.92, smearScale: 1,
      rattleHz: 218, brightness: 0.42, rattleScale: 0.85,
      modes: [
        { hz: 96, db: 2.6, q: 1.45 }, { hz: 218, db: 3.4, q: 2.15 },
        { hz: 465, db: -1.8, q: 1.25 }, { hz: 790, db: 1.6, q: 1.8 },
      ],
    },
    brick: {
      lpMin: 850, dipHz: 1280, dipDb: -4.1, bumpHz: 185, bumpDb: 1.5,
      damp: 0.82, leakBias: -0.05, leakTone: 0.26, loss: 0.76, smearScale: 0.62,
      rattleHz: 154, brightness: 0.18, rattleScale: 0.25,
      modes: [
        { hz: 72, db: 1.4, q: 1.1 }, { hz: 154, db: 2.1, q: 1.7 },
        { hz: 326, db: 1.2, q: 2.4 }, { hz: 680, db: -2.2, q: 1.35 },
      ],
    },
    wood: {
      lpMin: 1400, dipHz: 1370, dipDb: -2.2, bumpHz: 285, bumpDb: 2.2,
      damp: 0.55, leakBias: 0.03, leakTone: 0.62, loss: 0.9, smearScale: 1.12,
      rattleHz: 238, brightness: 0.5, rattleScale: 1,
      modes: [
        { hz: 88, db: 2.3, q: 1.25 }, { hz: 238, db: 4.1, q: 2.7 },
        { hz: 520, db: 2.2, q: 2.05 }, { hz: 1120, db: -1.8, q: 1.4 },
      ],
    },
    curtain: {
      lpMin: 2500, dipHz: 2400, dipDb: -1.2, bumpHz: 235, bumpDb: 0.5,
      damp: 0.72, leakBias: 0.16, leakTone: 0.68, loss: 0.98, smearScale: 0.35,
      rattleHz: 310, brightness: 0.3, rattleScale: 0.08,
      modes: [
        { hz: 115, db: 0.4, q: 0.8 }, { hz: 310, db: 0.7, q: 1.1 },
        { hz: 720, db: -1.1, q: 0.9 }, { hz: 2100, db: -2.4, q: 0.8 },
      ],
    },
    door: {
      lpMin: 1200, dipHz: 1220, dipDb: -2.8, bumpHz: 245, bumpDb: 2.5,
      damp: 0.61, leakBias: 0.09, leakTone: 0.58, loss: 0.87, smearScale: 1.18,
      rattleHz: 196, brightness: 0.46, rattleScale: 1.05,
      modes: [
        { hz: 78, db: 2.7, q: 1.3 }, { hz: 196, db: 4.6, q: 2.9 },
        { hz: 425, db: 2.5, q: 2.15 }, { hz: 930, db: -2.1, q: 1.4 },
      ],
    },
    glass: {
      lpMin: 2350, dipHz: 1420, dipDb: -1.4, bumpHz: 820, bumpDb: 2.4,
      damp: 0.35, leakBias: 0.12, leakTone: 0.82, loss: 0.88, smearScale: 1.3,
      rattleHz: 980, brightness: 0.92, rattleScale: 1.15,
      modes: [
        { hz: 164, db: 1.5, q: 2.6 }, { hz: 470, db: 3.2, q: 4.2 },
        { hz: 980, db: 4.4, q: 5.2 }, { hz: 2260, db: 2.3, q: 4.5 },
      ],
    },
    metal: {
      lpMin: 2100, dipHz: 1720, dipDb: -1.7, bumpHz: 610, bumpDb: 3.2,
      damp: 0.27, leakBias: 0.04, leakTone: 0.76, loss: 0.84, smearScale: 1.45,
      rattleHz: 720, brightness: 1, rattleScale: 1.65,
      modes: [
        { hz: 142, db: 2.5, q: 2.8 }, { hz: 610, db: 6.5, q: 5.4 },
        { hz: 1380, db: 5.5, q: 6.2 }, { hz: 3180, db: 4, q: 7.1 },
      ],
    },
    concrete: {
      lpMin: 720, dipHz: 1050, dipDb: -4.6, bumpHz: 118, bumpDb: 1.1,
      damp: 0.88, leakBias: -0.075, leakTone: 0.18, loss: 0.68, smearScale: 0.48,
      rattleHz: 112, brightness: 0.12, rattleScale: 0.12,
      modes: [
        { hz: 54, db: 1.1, q: 1 }, { hz: 112, db: 1.6, q: 1.4 },
        { hz: 245, db: 0.8, q: 1.8 }, { hz: 520, db: -2.7, q: 1.15 },
      ],
    },
  };
  return materials[String(material || "drywall")] || materials.drywall;
}

function constructionDefaults(construction) {
  const profiles = {
    solid: { cavity: 0.025, rattle: 0.01, looseness: 0.06, cavityHz: 135, resonanceBias: -0.08, smear: 0.78 },
    stud: { cavity: 0.52, rattle: 0.09, looseness: 0.34, cavityHz: 235, resonanceBias: 0.06, smear: 1 },
    hollow: { cavity: 0.76, rattle: 0.18, looseness: 0.52, cavityHz: 165, resonanceBias: 0.12, smear: 1.18 },
    panel: { cavity: 0.38, rattle: 0.4, looseness: 0.65, cavityHz: 340, resonanceBias: 0.16, smear: 1.25 },
    loose: { cavity: 0.62, rattle: 0.78, looseness: 0.9, cavityHz: 285, resonanceBias: 0.22, smear: 1.4 },
  };
  return profiles[String(construction || "stud")] || profiles.stud;
}

export function defaultSettings() {
  return {
    distance: 0.35, wall: 0.45, material: "drywall", construction: "stud", sourceRoom: 0.35, listenerRoom: 0.45,
    hpHz: 58, lpHz: 5200, dipHz: 1550, dipDb: -2.4, dipQ: 1.1,
    bumpHz: 350, bumpDb: 1.4, bumpQ: 0.95,
    resonance: 0.48, cavity: 0.52, rattle: 0.08, looseness: 0.34,
    smear: 0.38, leak: 0.08, leakTone: 0.52,
    roomMix: 0.22, predelayMs: 8, roomSize: 0.5, damp: 0.63, outGain: 1,
  };
}

function computeMacroTargets(s, ctx) {
  const dist = clamp01(s.distance ?? 0.35);
  const wall = clamp01(s.wall ?? 0.45);
  const srcRoom = clamp01(s.sourceRoom ?? 0.35);
  const lisRoom = clamp01(s.listenerRoom ?? 0.45);
  const mat = materialDefaults(s.material);
  const build = constructionDefaults(s.construction);
  const d = Math.pow(dist, 1.15);
  const w = Math.pow(wall, 1.18);
  const room = clamp01(0.42 * srcRoom + 0.58 * lisRoom);

  // Dense boundaries primarily remove highs. This HP only trims sub-energy
  // a finite panel cannot reproduce; it is not the identity of the effect.
  const hpHz = 24 + d * 38 + w * 32;
  const openLp = 17800 - d * 3800;
  // Transmission bandwidth must converge toward the material's physical
  // floor as the boundary closes. The previous max() formula kept dense walls
  // unexpectedly bright until the final few percent of the control.
  const transmissionOpen = Math.pow(1 - w, 2.2);
  const lpHz = mat.lpMin + (openLp - mat.lpMin) * transmissionOpen;
  const dipDb = mat.dipDb - w * 1.5;
  const bumpDb = mat.bumpDb + w * 1.8;
  const dipHz = mat.dipHz * (0.95 + room * 0.1);
  const bumpHz = mat.bumpHz * (0.94 + (1 - room) * 0.12);
  const leak = clamp01(0.015 + (1 - w) * 0.13 + mat.leakBias);
  const leakTone = clamp01(mat.leakTone + (1 - w) * 0.08);
  const resonance = clamp01(0.2 + w * 0.4 + mat.smearScale * 0.07 + build.resonanceBias);
  const cavity = clamp01(build.cavity * (0.72 + w * 0.28));
  const rattle = clamp01(build.rattle * mat.rattleScale * (0.58 + w * 0.42));
  const looseness = clamp01(build.looseness);
  const smear = clamp01((0.1 + w * 0.4 * mat.smearScale + d * 0.08) * build.smear);
  const roomMix = clamp01(0.08 + room * 0.3 + d * 0.08);
  const predelayMs = Math.round(2 + lisRoom * 10 + d * 5);
  const damp = clamp01(mat.damp + room * 0.08);
  const roomSize = room;
  const outGain = Math.round((1 - d * 0.12 - w * 0.08) * 100) / 100;

  return {
    hpHz: hzClamp(ctx, hpHz, { min: 10 }), lpHz: hzClamp(ctx, lpHz, { min: 80 }),
    dipHz: hzClamp(ctx, dipHz, { min: 80 }), bumpHz: hzClamp(ctx, bumpHz, { min: 60 }),
    dipDb, bumpDb, leak, leakTone, resonance, cavity, rattle, looseness, smear,
    roomMix, predelayMs, damp, roomSize, outGain,
  };
}

function makeEarlyReflectionStage(ctx, baseTimesMs) {
  const input = new GainNode(ctx, { gain: 1 });
  const direct = new GainNode(ctx, { gain: 1 });
  const sum = new GainNode(ctx, { gain: 1 });
  const normalize = new GainNode(ctx, { gain: 1 });
  const taps = baseTimesMs.map((baseMs, index) => {
    const delay = new DelayNode(ctx, { delayTime: baseMs / 1000, maxDelayTime: 0.08 });
    const filter = new BiquadFilterNode(ctx, { type: "lowpass", frequency: 4200, Q: 0.707 });
    const gain = new GainNode(ctx, { gain: 0 });
    input.connect(delay); delay.connect(filter); filter.connect(gain); gain.connect(sum);
    return { delay, filter, gain, baseMs, weight: [0.30, 0.22, 0.16, 0.11][index] || 0.08 };
  });
  input.connect(direct); direct.connect(sum); sum.connect(normalize);

  function apply({ amount = 0.3, size = 0.5, damping = 0.6, offsetMs = 0 } = {}, { time = now(ctx), ramp = 0.02 } = {}) {
    const a = clamp01(amount);
    const sz = clamp01(size);
    const damp = clamp01(damping);
    let tapPower = 0;
    for (const tap of taps) {
      const delayMs = Math.min(75, Math.max(0.4, offsetMs + tap.baseMs * (0.72 + sz * 0.75)));
      rampParam(tap.delay.delayTime, delayMs / 1000, time, ramp);
      rampParam(tap.filter.frequency, hzClamp(ctx, 650 + (1 - damp) * 9200, { min: 300 }), time, ramp);
      const gain = a * tap.weight;
      rampParam(tap.gain.gain, gain, time, ramp);
      tapPower += gain * gain;
    }
    const directValue = 1 - a * 0.10;
    rampParam(direct.gain, directValue, time, ramp);
    rampParam(normalize.gain, 1 / Math.sqrt(Math.max(0.72, directValue * directValue + tapPower)), time, ramp);
  }
  return { input, output: normalize, apply, nodes: { input, direct, sum, normalize, taps } };
}

function makeTransmissionSmear(ctx) {
  const input = new GainNode(ctx, { gain: 1 });
  const direct = new GainNode(ctx, { gain: 1 });
  const sum = new GainNode(ctx, { gain: 1 });
  const normalize = new GainNode(ctx, { gain: 1 });
  const specs = [
    { ms: 1.15, weight: 0.28 }, { ms: 2.85, weight: -0.2 },
    { ms: 5.6, weight: 0.14 }, { ms: 8.8, weight: -0.08 },
  ];
  const taps = specs.map((spec) => {
    const delay = new DelayNode(ctx, { delayTime: spec.ms / 1000, maxDelayTime: 0.024 });
    const filter = new BiquadFilterNode(ctx, { type: "lowpass", frequency: 4200, Q: 0.707 });
    const gain = new GainNode(ctx, { gain: 0 });
    input.connect(delay); delay.connect(filter); filter.connect(gain); gain.connect(sum);
    return { ...spec, delay, filter, gain };
  });
  input.connect(direct); direct.connect(sum); sum.connect(normalize);

  function apply({ amount = 0.35, scale = 1, damping = 0.6 } = {}, { time = now(ctx), ramp = 0.02 } = {}) {
    const a = clamp01(amount);
    const sc = Math.max(0.35, Math.min(1.8, Number(scale) || 1));
    let tapPower = 0;
    for (const tap of taps) {
      rampParam(tap.delay.delayTime, Math.min(0.023, (tap.ms * sc) / 1000), time, ramp);
      rampParam(tap.filter.frequency, hzClamp(ctx, 1000 + (1 - clamp01(damping)) * 6200, { min: 300 }), time, ramp);
      const gain = tap.weight * a;
      rampParam(tap.gain.gain, gain, time, ramp);
      tapPower += gain * gain;
    }
    const directValue = 1 - a * 0.08;
    rampParam(direct.gain, directValue, time, ramp);
    rampParam(normalize.gain, 1 / Math.sqrt(Math.max(0.68, directValue * directValue + tapPower)), time, ramp);
  }
  return { input, output: normalize, apply, nodes: { input, direct, sum, normalize, taps } };
}

function makeCavityStage(ctx) {
  const input = new GainNode(ctx, { gain: 1 });
  const direct = new GainNode(ctx, { gain: 1 });
  const cavityFilter = new BiquadFilterNode(ctx, { type: "bandpass", frequency: 420, Q: 1.8 });
  const delay = new DelayNode(ctx, { delayTime: 1 / 235, maxDelayTime: 0.03 });
  const feedbackTone = new BiquadFilterNode(ctx, { type: "lowpass", frequency: 3200, Q: 0.707 });
  const feedback = new GainNode(ctx, { gain: 0 });
  const wet = new GainNode(ctx, { gain: 0 });
  const sum = new GainNode(ctx, { gain: 1 });
  const normalize = new GainNode(ctx, { gain: 1 });

  input.connect(direct); direct.connect(sum);
  input.connect(cavityFilter); cavityFilter.connect(delay); delay.connect(wet); wet.connect(sum);
  delay.connect(feedbackTone); feedbackTone.connect(feedback); feedback.connect(delay);
  sum.connect(normalize);

  function apply({ amount = 0.4, fundamentalHz = 235, damping = 0.6, resonance = 0.5 } = {}, { time = now(ctx), ramp = 0.02 } = {}) {
    const a = clamp01(amount);
    const hz = Math.max(55, Math.min(1400, Number(fundamentalHz) || 235));
    const res = clamp01(resonance);
    rampParam(cavityFilter.frequency, hzClamp(ctx, hz * (1.72 + res * 0.7), { min: 80 }), time, ramp);
    rampParam(cavityFilter.Q, 0.75 + res * 1.4, time, ramp);
    rampParam(delay.delayTime, Math.min(0.028, 1 / hz), time, ramp);
    rampParam(feedbackTone.frequency, hzClamp(ctx, 850 + (1 - clamp01(damping)) * 6200, { min: 300 }), time, ramp);
    rampParam(feedback.gain, a * (0.3 + res * 0.5), time, ramp);
    rampParam(wet.gain, a * (0.4 + res), time, ramp);
    rampParam(direct.gain, 1 - a * 0.05, time, ramp);
    rampParam(normalize.gain, 1 / (1 + a * 0.2), time, ramp);
  }
  return { input, output: normalize, apply, nodes: { input, direct, cavityFilter, delay, feedbackTone, feedback, wet, sum, normalize } };
}

export async function buildOcclusionGraph(ctx, { seed } = {}) {
  await ensureBodyWorklet(ctx);
  const input = new GainNode(ctx, { gain: 1 });
  const sourceRoom = makeEarlyReflectionStage(ctx, [2.1, 4.8, 8.6]);
  const hp1 = new BiquadFilterNode(ctx, { type: "highpass", frequency: 58, Q: 0.707 });
  const hp2 = new BiquadFilterNode(ctx, { type: "highpass", frequency: 58, Q: 0.707 });
  const bump = new BiquadFilterNode(ctx, { type: "peaking", frequency: 350, Q: 0.95, gain: 1.4 });
  const dip = new BiquadFilterNode(ctx, { type: "peaking", frequency: 1550, Q: 1.1, gain: -2.4 });
  const modes = Array.from({ length: 4 }, () => new BiquadFilterNode(ctx, { type: "peaking", frequency: 300, Q: 1.5, gain: 0 }));
  const lp1 = new BiquadFilterNode(ctx, { type: "lowpass", frequency: 5200, Q: 0.82 });
  const lp2 = new BiquadFilterNode(ctx, { type: "lowpass", frequency: 5200, Q: 0.82 });
  const cavity = makeCavityStage(ctx);
  const smear = makeTransmissionSmear(ctx);
  const bodyGain = new GainNode(ctx, { gain: 0.8 });
  const bodyExciter = new AudioWorkletNode(ctx, "obfuscation-body", {
    numberOfInputs: 1,
    numberOfOutputs: 1,
    outputChannelCount: [1],
    processorOptions: { seed: (seed ?? 0x6f626675) >>> 0 },
  });
  const rattleGain = new GainNode(ctx, { gain: 0.72 });
  const listenerRoom = makeEarlyReflectionStage(ctx, [3.2, 7.4, 13.6, 21.4]);
  const leakHp = new BiquadFilterNode(ctx, { type: "highpass", frequency: 220, Q: 0.707 });
  const leakLp = new BiquadFilterNode(ctx, { type: "lowpass", frequency: 6200, Q: 0.707 });
  const leakDelay = new DelayNode(ctx, { delayTime: 0.0014, maxDelayTime: 0.012 });
  const leakGain = new GainNode(ctx, { gain: 0.08 });
  const sum = new GainNode(ctx, { gain: 1 });
  const outGain = new GainNode(ctx, { gain: 1 });

  input.connect(sourceRoom.input); sourceRoom.output.connect(hp1); hp1.connect(hp2); hp2.connect(bump); bump.connect(dip);
  let bodyTail = dip;
  for (const mode of modes) { bodyTail.connect(mode); bodyTail = mode; }
  bodyTail.connect(lp1); lp1.connect(lp2); lp2.connect(cavity.input); cavity.output.connect(smear.input);
  smear.output.connect(bodyGain); bodyGain.connect(listenerRoom.input);
  smear.output.connect(bodyExciter); bodyExciter.connect(rattleGain); rattleGain.connect(listenerRoom.input);
  listenerRoom.output.connect(sum);

  // A crack or door-edge leak is a delayed, band-limited alternate route. It
  // can reveal articulation, but it can never become the old full-band bypass.
  input.connect(leakHp); leakHp.connect(leakLp); leakLp.connect(leakDelay); leakDelay.connect(leakGain); leakGain.connect(sum);
  sum.connect(outGain);

  function reset() {
    bodyExciter.port.postMessage({ type: "reset", seed: (seed ?? 0x6f626675) >>> 0 });
  }

  function applySettings(settings, { time = now(ctx), ramp = 0.02 } = {}) {
    const incoming = settings || {};
    const s = { ...defaultSettings(), ...incoming };
    const macro = computeMacroTargets(s, ctx);
    const mat = materialDefaults(s.material);
    const build = constructionDefaults(s.construction);
    const explicit = (key) => Object.prototype.hasOwnProperty.call(incoming, key);
    const pick = (key) => (explicit(key) ? s[key] : macro[key]);
    const hpHz = hzClamp(ctx, pick("hpHz"), { min: 10 });
    const lpHz = hzClamp(ctx, pick("lpHz"), { min: 80 });
    for (const h of [hp1, hp2]) rampParam(h.frequency, hpHz, time, ramp);
    for (const l of [lp1, lp2]) rampParam(l.frequency, lpHz, time, ramp);
    rampParam(bump.frequency, hzClamp(ctx, pick("bumpHz"), { min: 60 }), time, ramp);
    rampParam(bump.Q, Math.max(0.2, Math.min(5, Number(s.bumpQ))), time, ramp);
    rampParam(bump.gain, Math.max(-12, Math.min(12, Number(pick("bumpDb")))), time, ramp);
    rampParam(dip.frequency, hzClamp(ctx, pick("dipHz"), { min: 80 }), time, ramp);
    rampParam(dip.Q, Math.max(0.2, Math.min(8, Number(s.dipQ))), time, ramp);
    rampParam(dip.gain, Math.max(-18, Math.min(6, Number(pick("dipDb")))), time, ramp);

    const resonance = clamp01(pick("resonance"));
    const wall = clamp01(s.wall);
    const roomShift = 0.94 + (1 - clamp01(s.sourceRoom)) * 0.12;
    for (let i = 0; i < modes.length; i++) {
      const spec = mat.modes[i];
      const strength = resonance * (0.65 + wall * 0.85) * (0.9 + mat.brightness * 0.35);
      rampParam(modes[i].frequency, hzClamp(ctx, spec.hz * roomShift, { min: 35 }), time, ramp);
      rampParam(modes[i].Q, Math.max(0.55, Math.min(7, spec.q * (0.82 + resonance * 0.45))), time, ramp);
      rampParam(modes[i].gain, Math.max(-9, Math.min(9, spec.db * strength)), time, ramp);
    }

    const damping = clamp01(pick("damp"));
    const roomMix = clamp01(pick("roomMix"));
    const roomSize = clamp01(pick("roomSize"));
    const predelayMs = Math.max(0, Math.min(28, Number(pick("predelayMs")) || 0));
    sourceRoom.apply(
      { amount: roomMix * clamp01(s.sourceRoom) * 1.45, size: clamp01(s.sourceRoom), damping, offsetMs: 0 }, { time, ramp },
    );
    listenerRoom.apply(
      { amount: roomMix * clamp01(s.listenerRoom) * 1.55, size: roomSize, damping, offsetMs: predelayMs * 0.55 }, { time, ramp },
    );
    const smearAmount = clamp01(pick("smear"));
    smear.apply({ amount: smearAmount, scale: mat.smearScale * (0.82 + wall * 0.42), damping }, { time, ramp });
    const cavityAmount = clamp01(pick("cavity"));
    cavity.apply(
      { amount: cavityAmount, fundamentalHz: build.cavityHz * (0.86 + wall * 0.3), damping, resonance },
      { time, ramp },
    );

    const rattle = clamp01(pick("rattle"));
    const looseness = clamp01(pick("looseness"));
    rampParam(bodyExciter.parameters.get("amount"), rattle, time, ramp);
    rampParam(bodyExciter.parameters.get("looseness"), looseness, time, ramp);
    rampParam(bodyExciter.parameters.get("brightness"), mat.brightness, time, ramp);
    rampParam(bodyExciter.parameters.get("bodyHz"), hzClamp(ctx, mat.rattleHz * (0.88 + wall * 0.24), { min: 40 }), time, ramp);
    rampParam(bodyExciter.parameters.get("cavityHz"), hzClamp(ctx, build.cavityHz * (1.5 + mat.brightness * 1.4), { min: 50 }), time, ramp);
    rampParam(rattleGain.gain, 0.78 + mat.brightness * 0.22, time, ramp);
    const distance = clamp01(s.distance);
    const bodyLevel = mat.loss * (0.88 - wall * 0.13 - distance * 0.09);
    rampParam(bodyGain.gain, Math.max(0.42, Math.min(1, bodyLevel)), time, ramp);

    const leakTone = clamp01(pick("leakTone"));
    rampParam(leakHp.frequency, hzClamp(ctx, 95 + leakTone * 410 + wall * 90, { min: 50 }), time, ramp);
    rampParam(leakLp.frequency, hzClamp(ctx, 2300 + leakTone * 8800 - wall * 900, { min: 900 }), time, ramp);
    rampParam(leakDelay.delayTime, Math.min(0.011, (0.55 + wall * 1.9 + mat.smearScale * 0.45) / 1000), time, ramp);
    rampParam(leakGain.gain, clamp01(pick("leak")) * (0.72 + leakTone * 0.28), time, ramp);
    rampParam(outGain.gain, Math.max(0, Math.min(1.5, Number(pick("outGain")))), time, ramp);
  }

  return {
    input, output: outGain,
    nodes: { input, sourceRoom, hp1, hp2, bump, dip, modes, lp1, lp2, cavity, smear, bodyGain, bodyExciter, rattleGain, listenerRoom, leakHp, leakLp, leakDelay, leakGain, sum, outGain },
    reset, applySettings,
  };
}
