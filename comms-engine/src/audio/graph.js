const WORKLET_URL = new URL("./comms-processor.js", import.meta.url);

function now(ctx) {
  return ctx.currentTime;
}

export async function ensureWorklet(ctx) {
  if (ctx.__commsWorkletLoaded) return;
  await ctx.audioWorklet.addModule(WORKLET_URL.href);
  ctx.__commsWorkletLoaded = true;
}

export function defaultSettings() {
  return {
    mode: "landline",
    bandwidth: 0.4,
    drive: 0.35,
    glitch: 0.2,
    noise: 0.18,
    alarmTone: false,

    hpHz: 280,
    lpHz: 3400,
    midHumpDb: 3.5,
    midFreq: 1850,
    comp: 0.45,
    bits: 12,
    rate: 24000,
    packet: 0.2,
    packetMs: 28,
    hum: 0.25,
    hiss: 0.22,
    toneMix: 0.35,
    ceiling: 0.92,
    outGain: 0.95,

    echoMix: 0,
    echoMs: 180,
    echoFb: 0.28,
    echoTone: 0.55,

    verbMix: 0,
    verbMs: 240,
    verbDamp: 0.45,
  };
}

function modeToIndex(mode) {
  if (typeof mode === "number") return Math.round(Math.max(0, Math.min(4, mode)));
  if (mode === "cell") return 1;
  if (mode === "intercom") return 2;
  if (mode === "pa") return 3;
  if (mode === "alarm") return 4;
  return 0;
}

export async function buildCommsGraph(ctx, { seed }) {
  await ensureWorklet(ctx);

  const input = new GainNode(ctx, { gain: 1 });

  const hp1 = new BiquadFilterNode(ctx, { type: "highpass", Q: 0.707, frequency: 280 });
  const hp2 = new BiquadFilterNode(ctx, { type: "highpass", Q: 0.707, frequency: 280 });
  const dip = new BiquadFilterNode(ctx, { type: "peaking", frequency: 650, Q: 0.9, gain: 0 });
  const hump = new BiquadFilterNode(ctx, { type: "peaking", frequency: 1850, Q: 1.25, gain: 3.5 });
  const lp1 = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.85, frequency: 3400 });
  const lp2 = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.85, frequency: 3400 });

  const processor = new AudioWorkletNode(ctx, "comms", {
    numberOfInputs: 1,
    numberOfOutputs: 1,
    outputChannelCount: [1],
    processorOptions: { seed: seed >>> 0 },
  });

  const echoDelay = new DelayNode(ctx, { delayTime: 0.18, maxDelayTime: 2.5 });
  const echoFbFilter = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.707, frequency: 2800 });
  const echoFbGain = new GainNode(ctx, { gain: 0.28 });
  const echoWet = new GainNode(ctx, { gain: 0 });

  const convolver = new ConvolverNode(ctx, { disableNormalization: false });
  const verbDampFilter = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.707, frequency: 6000 });
  const verbWet = new GainNode(ctx, { gain: 0 });

  const dryGain = new GainNode(ctx, { gain: 1 });
  const sum = new GainNode(ctx, { gain: 1 });
  const out = new GainNode(ctx, { gain: 1 });

  input.connect(hp1);
  hp1.connect(hp2);
  hp2.connect(dip);
  dip.connect(hump);
  hump.connect(lp1);
  lp1.connect(lp2);
  lp2.connect(processor);

  // Echo (parallel)
  processor.connect(echoDelay);
  echoDelay.connect(echoWet);
  echoDelay.connect(echoFbFilter);
  echoFbFilter.connect(echoFbGain);
  echoFbGain.connect(echoDelay);

  // Room (parallel)
  processor.connect(convolver);
  convolver.connect(verbDampFilter);
  verbDampFilter.connect(verbWet);

  // Dry + wet sum
  processor.connect(dryGain);
  dryGain.connect(sum);
  echoWet.connect(sum);
  verbWet.connect(sum);
  sum.connect(out);

  function makeRoomIR(ms, seedLocal) {
    const sr = ctx.sampleRate;
    const len = Math.max(32, Math.floor((Math.max(20, Math.min(2500, ms)) / 1000) * sr));
    const buf = ctx.createBuffer(1, len, sr);
    const ch = buf.getChannelData(0);

    // Simple deterministic noise -> exponential decay, with one-pole lowpass for "damp".
    let state = (seedLocal >>> 0) || 0x12345678;
    const rnd = () => {
      // xorshift32
      let x = state >>> 0;
      x ^= x << 13;
      x ^= x >>> 17;
      x ^= x << 5;
      state = x >>> 0;
      return (state >>> 0) / 0xffffffff;
    };

    // Keep IR generation independent of the "damp" knob; damp is applied post-convolver.
    const dampHz = 9000;
    const a = Math.exp((-2 * Math.PI * dampHz) / sr);
    let lp = 0;
    let peak = 1e-6;
    for (let i = 0; i < len; i++) {
      const t = i / (len - 1);
      const env = Math.pow(1 - t, 2.2);
      let x = (rnd() * 2 - 1) * env;
      lp = (1 - a) * x + a * lp;
      ch[i] = lp;
      peak = Math.max(peak, Math.abs(lp));
    }
    const g = 0.75 / peak;
    for (let i = 0; i < len; i++) ch[i] *= g;
    return buf;
  }

  let currentIrMs = null;
  function ensureIrFor(s, seedLocal) {
    const ms = Math.max(35, Math.min(2500, s.verbMs ?? 240));
    if (currentIrMs === ms && convolver.buffer) return;
    currentIrMs = ms;
    const irSeed = ((seedLocal >>> 0) ^ 0x6a09e667 ^ (ms * 2654435761)) >>> 0;
    convolver.buffer = makeRoomIR(ms, irSeed);
  }

  function reset(seedNext) {
    processor.port.postMessage({ type: "reset", seed: seedNext >>> 0 });
  }

  function applySettings(settings, { time = now(ctx), ramp = 0.02 } = {}) {
    const t1 = time + ramp;
    const s = { ...settings };

    const hpHz = Math.max(40, Math.min(1200, s.hpHz ?? 280));
    const lpHz = Math.max(800, Math.min(14000, s.lpHz ?? 3400));
    const midDb = Math.max(0, Math.min(14, s.midHumpDb ?? 3.5));
    const midF = Math.max(600, Math.min(5000, s.midFreq ?? 1850));

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

    processor.parameters.get("drive")?.setValueAtTime(s.drive ?? 0.35, time);
    processor.parameters.get("comp")?.setValueAtTime(s.comp ?? 0.45, time);
    processor.parameters.get("bits")?.setValueAtTime(s.bits ?? 12, time);
    processor.parameters.get("rate")?.setValueAtTime(s.rate ?? 24000, time);
    processor.parameters.get("packet")?.setValueAtTime(s.packet ?? 0.2, time);
    processor.parameters.get("packetMs")?.setValueAtTime(s.packetMs ?? 28, time);
    processor.parameters.get("hum")?.setValueAtTime(s.hum ?? 0.25, time);
    processor.parameters.get("hiss")?.setValueAtTime(s.hiss ?? 0.22, time);
    processor.parameters.get("toneMix")?.setValueAtTime(s.toneMix ?? 0.35, time);
    processor.parameters.get("alarm")?.setValueAtTime(s.alarmTone ? 1 : 0, time);
    processor.parameters.get("mode")?.setValueAtTime(modeToIndex(s.mode), time);
    processor.parameters.get("ceiling")?.setValueAtTime(s.ceiling ?? 0.92, time);
    processor.parameters.get("outGain")?.setValueAtTime(s.outGain ?? 0.95, time);

    // Echo
    const echoMix = Math.max(0, Math.min(1, s.echoMix ?? 0));
    const echoMs = Math.max(10, Math.min(2500, s.echoMs ?? 180));
    const echoFb = Math.max(0, Math.min(0.92, s.echoFb ?? 0.28));
    const echoTone = Math.max(0, Math.min(1, s.echoTone ?? 0.55));
    const echoWetGain = Math.min(1.25, echoMix * 0.95);
    echoWet.gain.cancelScheduledValues(time);
    echoWet.gain.setValueAtTime(echoWet.gain.value, time);
    echoWet.gain.linearRampToValueAtTime(echoWetGain, t1);
    echoDelay.delayTime.cancelScheduledValues(time);
    echoDelay.delayTime.setValueAtTime(echoDelay.delayTime.value, time);
    echoDelay.delayTime.linearRampToValueAtTime(echoMs / 1000, t1);
    echoFbGain.gain.cancelScheduledValues(time);
    echoFbGain.gain.setValueAtTime(echoFbGain.gain.value, time);
    echoFbGain.gain.linearRampToValueAtTime(echoFb, t1);
    const echoCut = 900 + echoTone * 7500;
    echoFbFilter.frequency.cancelScheduledValues(time);
    echoFbFilter.frequency.setValueAtTime(echoFbFilter.frequency.value, time);
    echoFbFilter.frequency.linearRampToValueAtTime(echoCut, t1);

    // Room
    const verbMix = Math.max(0, Math.min(1, s.verbMix ?? 0));
    const verbWetGain = Math.min(1.35, verbMix * 1.15);
    verbWet.gain.cancelScheduledValues(time);
    verbWet.gain.setValueAtTime(verbWet.gain.value, time);
    verbWet.gain.linearRampToValueAtTime(verbWetGain, t1);
    const damp = Math.max(0, Math.min(1, s.verbDamp ?? 0.45));
    ensureIrFor(s, seed >>> 0);
    const verbCut = 1600 + (1 - damp) * 12000;
    verbDampFilter.frequency.cancelScheduledValues(time);
    verbDampFilter.frequency.setValueAtTime(verbDampFilter.frequency.value, time);
    verbDampFilter.frequency.linearRampToValueAtTime(verbCut, t1);

    // Gentle dry attenuation as wet increases so the effect is obvious.
    const wetAmt = Math.max(echoMix, verbMix);
    const dry = Math.max(0.55, 1 - wetAmt * 0.5);
    dryGain.gain.cancelScheduledValues(time);
    dryGain.gain.setValueAtTime(dryGain.gain.value, time);
    dryGain.gain.linearRampToValueAtTime(dry, t1);
  }

  return {
    input,
    output: out,
    nodes: {
      input,
      hp1,
      hp2,
      dip,
      hump,
      lp1,
      lp2,
      processor,
      echoDelay,
      echoFbFilter,
      echoFbGain,
      echoWet,
      convolver,
      verbDampFilter,
      verbWet,
      dryGain,
      sum,
      out,
    },
    reset,
    applySettings,
  };
}
