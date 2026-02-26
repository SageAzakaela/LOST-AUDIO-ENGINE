const WORKLET_URL = new URL("./transmission-processor.js", import.meta.url);
function now(ctx) {
  return ctx.currentTime;
}
export async function ensureWorklet(ctx) {
  if (ctx.__transmissionWorkletLoaded) return;
  await ctx.audioWorklet.addModule(WORKLET_URL.href);
  ctx.__transmissionWorkletLoaded = true;
}
export function mapBandwidth(bw) {
  const hp = 600 - bw * 400;
  const lp = 2500 + bw * 3500;
  const midGainDb = (1 - bw) * 5.2;
  const midQ = 0.9 + (1 - bw) * 1.2;
  return { hp, lp, midGainDb, midQ, midFreq: 1550 };
}
export function defaultSettings() {
  return {
    bandwidth: 0.45,
    drive: 0.35,
    badConnection: 0.25,
    noiseProfile: 0.2,
    pinkNoise: false,
    walkieMode: false,
    // Advanced defaults (UI macros keep these in sync).
    hpHz: 380,
    lpHz: 5200,
    midGainDb: 0,
    midFreq: 1550,
    midQ: 1.2,
    boxDipDb: 0,
    comp: 0.25,
    asym: 0.1,
    preDrive: 0.25,
    postDrive: 0.35,
    crush: 0,
    wowDepth: 0.25,
    dropRate: 0.25,
    dropDepth: 0.35,
    crackle: 0.25,
    lfoRate: 0.7,
    noiseColor: 0,
    hiss: 0.2,
    walkieThresholdDb: -45,
    walkieMinSilenceMs: 220,
    walkieClickMs: 12,
    walkieClickLevel: 0.65,
    walkieFx: "click",
    outGain: 0.92,
    passes: 1,
    tuningEnable: false,
    tuningMode: "edges",
    tuningSource: "synth",
    tuningAmount: 0.35,
    tuningSnippetMs: 140,
    tuningCutDepth: 0.55,
  };
}

function mixSeed(base, salt) {
  let x = (base ^ salt) >>> 0;
  x = Math.imul(x ^ (x >>> 16), 0x7feb352d) >>> 0;
  x = Math.imul(x ^ (x >>> 15), 0x846ca68b) >>> 0;
  return (x ^ (x >>> 16)) >>> 0;
}
export async function buildTransmissionGraph(ctx, { seed, passes = 1, tuningEdges = null, tuningSample = null }) {
  await ensureWorklet(ctx);
  const clickNode = new AudioWorkletNode(ctx, "walkie-click", {
    numberOfInputs: 1,
    numberOfOutputs: 1,
    outputChannelCount: [1],
    processorOptions: { seed: mixSeed(seed, 0xa11ce001) },
  });
  const tuningNode = new AudioWorkletNode(ctx, "tuning-noise", {
    numberOfInputs: 1,
    numberOfOutputs: 1,
    outputChannelCount: [1],
    processorOptions: {
      seed: mixSeed(seed, 0x71c1_9e51),
      leadEnd: tuningEdges?.leadEnd ?? 0,
      tailStart: tuningEdges?.tailStart ?? Number.POSITIVE_INFINITY,
      sampleRate: tuningSample?.sampleRate ?? 0,
      sampleData: tuningSample?.data ?? null,
    },
  });
  const out = new GainNode(ctx, { gain: 1 });

  const stages = [];
  const passCount = Math.max(1, Math.min(12, Math.floor(passes)));
  for (let i = 0; i < passCount; i++) {
    const preSat = new AudioWorkletNode(ctx, "transmission-sat", {
      numberOfInputs: 1,
      numberOfOutputs: 1,
      outputChannelCount: [1],
    });
    const hp1 = new BiquadFilterNode(ctx, { type: "highpass", Q: 0.9, frequency: 380 });
    const hp2 = new BiquadFilterNode(ctx, { type: "highpass", Q: 0.9, frequency: 380 });
    const lp1 = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.9, frequency: 5200 });
    const lp2 = new BiquadFilterNode(ctx, { type: "lowpass", Q: 0.9, frequency: 5200 });
    const dip = new BiquadFilterNode(ctx, { type: "peaking", frequency: 680, Q: 0.8, gain: 0 });
    const mid = new BiquadFilterNode(ctx, { type: "peaking", frequency: 1550, Q: 1.2, gain: 0 });
    const postNode = new AudioWorkletNode(ctx, "transmission-post", {
      numberOfInputs: 1,
      numberOfOutputs: 1,
      outputChannelCount: [1],
      processorOptions: { seed: mixSeed(seed, 0x70005700 ^ (i * 0x9e3779b9)) },
    });
    stages.push({ preSat, hp1, hp2, lp1, lp2, dip, mid, postNode });
  }

  let head = clickNode;
  head.connect(tuningNode);
  head = tuningNode;
  for (const st of stages) {
    head.connect(st.preSat);
    st.preSat.connect(st.hp1);
    st.hp1.connect(st.hp2);
    st.hp2.connect(st.lp1);
    st.lp1.connect(st.lp2);
    st.lp2.connect(st.dip);
    st.dip.connect(st.mid);
    st.mid.connect(st.postNode);
    head = st.postNode;
  }
  head.connect(out);

  function reset(seedNext) {
    clickNode.port.postMessage({ type: "reset", seed: mixSeed(seedNext >>> 0, 0xa11ce001) });
    tuningNode.port.postMessage({ type: "reset", seed: mixSeed(seedNext >>> 0, 0x71c1_9e51) });
    for (let i = 0; i < stages.length; i++) {
      stages[i].postNode.port.postMessage({ type: "reset", seed: mixSeed(seedNext >>> 0, 0x70005700 ^ (i * 0x9e3779b9)) });
    }
  }
  function applySettings(settings, { time = now(ctx), ramp = 0.02 } = {}) {
    const t1 = time + ramp;
    const mapped = mapBandwidth(settings.bandwidth ?? 0.45);
    const hpHz = settings.hpHz ?? mapped.hp;
    const lpHz = settings.lpHz ?? mapped.lp;
    const midGainDb = settings.midGainDb ?? mapped.midGainDb;
    const midQ = settings.midQ ?? mapped.midQ;
    const midFreq = settings.midFreq ?? mapped.midFreq;
    const dipDb = settings.boxDipDb ?? 0;
    const bw = settings.bandwidth ?? 0.45;
    const filtQ = 0.9 + (1 - bw) * 0.35;

    for (const st of stages) {
      for (const hp of [st.hp1, st.hp2]) {
        hp.Q.setValueAtTime(filtQ, time);
        hp.frequency.cancelScheduledValues(time);
        hp.frequency.setValueAtTime(hp.frequency.value, time);
        hp.frequency.linearRampToValueAtTime(hpHz, t1);
      }
      for (const lp of [st.lp1, st.lp2]) {
        lp.Q.setValueAtTime(filtQ, time);
        lp.frequency.cancelScheduledValues(time);
        lp.frequency.setValueAtTime(lp.frequency.value, time);
        lp.frequency.linearRampToValueAtTime(lpHz, t1);
      }

      st.dip.frequency.setValueAtTime(680, time);
      st.dip.Q.setValueAtTime(0.8, time);
      st.dip.gain.cancelScheduledValues(time);
      st.dip.gain.setValueAtTime(st.dip.gain.value, time);
      st.dip.gain.linearRampToValueAtTime(-Math.abs(dipDb), t1);

      st.mid.frequency.setValueAtTime(midFreq, time);
      st.mid.Q.cancelScheduledValues(time);
      st.mid.Q.setValueAtTime(st.mid.Q.value, time);
      st.mid.Q.linearRampToValueAtTime(midQ, t1);
      st.mid.gain.cancelScheduledValues(time);
      st.mid.gain.setValueAtTime(st.mid.gain.value, time);
      st.mid.gain.linearRampToValueAtTime(midGainDb, t1);
    }
    const drive = settings.drive ?? 0.35;
    const preDrive = settings.preDrive ?? Math.pow(drive, 0.85) * 0.75;
    const postDrive = settings.postDrive ?? drive;
    for (const st of stages) {
      st.preSat.parameters.get("drive")?.setValueAtTime(preDrive, time);
      st.preSat.parameters.get("asym")?.setValueAtTime(settings.asym ?? drive * 0.6, time);
      st.preSat.parameters.get("mix")?.setValueAtTime(1, time);

      st.postNode.parameters.get("drive")?.setValueAtTime(postDrive, time);
      st.postNode.parameters.get("asym")?.setValueAtTime(settings.asym ?? drive * 0.6, time);
      st.postNode.parameters.get("comp")?.setValueAtTime(settings.comp ?? (0.18 + drive * 0.65), time);
      st.postNode.parameters.get("crush")?.setValueAtTime(settings.crush ?? 0, time);
    }

    const bad = settings.badConnection ?? 0.25;
    for (const st of stages) {
      st.postNode.parameters.get("badAmount")?.setValueAtTime(bad, time);
      st.postNode.parameters.get("wowDepth")?.setValueAtTime(settings.wowDepth ?? bad, time);
      st.postNode.parameters.get("dropRate")?.setValueAtTime(settings.dropRate ?? bad, time);
      st.postNode.parameters.get("dropDepth")?.setValueAtTime(settings.dropDepth ?? bad, time);
      st.postNode.parameters.get("crackle")?.setValueAtTime(settings.crackle ?? bad, time);
      st.postNode.parameters.get("lfoRate")?.setValueAtTime(settings.lfoRate ?? (0.45 + bad * 1.6), time);
    }

    const noise = settings.noiseProfile ?? 0.2;
    for (const st of stages) {
      st.postNode.parameters.get("noise")?.setValueAtTime(noise, time);
      st.postNode.parameters.get("hiss")?.setValueAtTime(settings.hiss ?? noise * 0.95, time);
      st.postNode.parameters.get("noiseColor")?.setValueAtTime(
        settings.noiseColor ?? (settings.pinkNoise ? 1 : Math.max(0, (noise - 0.55) * 2)),
        time,
      );
      st.postNode.parameters.get("outGain")?.setValueAtTime(settings.outGain ?? 0.92, time);
    }

    clickNode.parameters.get("walkieEnable")?.setValueAtTime(settings.walkieMode ? 1 : 0, time);
    clickNode.parameters.get("thresholdDb")?.setValueAtTime(settings.walkieThresholdDb ?? -45, time);
    clickNode.parameters.get("minSilenceMs")?.setValueAtTime(settings.walkieMinSilenceMs ?? 220, time);
    clickNode.parameters.get("clickMs")?.setValueAtTime(settings.walkieClickMs ?? 12, time);
    clickNode.parameters.get("clickLevel")?.setValueAtTime(settings.walkieClickLevel ?? 0.65, time);
    clickNode.parameters.get("dispatchMode")?.setValueAtTime(settings.walkieFx === "dispatch" ? 1 : 0, time);

    tuningNode.parameters.get("enable")?.setValueAtTime(settings.tuningEnable ? 1 : 0, time);
    tuningNode.parameters.get("mode")?.setValueAtTime(settings.tuningMode === "search" ? 1 : 0, time);
    tuningNode.parameters.get("source")?.setValueAtTime(settings.tuningSource && settings.tuningSource !== "synth" ? 1 : 0, time);
    tuningNode.parameters.get("amount")?.setValueAtTime(settings.tuningAmount ?? 0.35, time);
    tuningNode.parameters.get("snippetMs")?.setValueAtTime(settings.tuningSnippetMs ?? 140, time);
    tuningNode.parameters.get("cutDepth")?.setValueAtTime(settings.tuningCutDepth ?? 0.55, time);
  }
  return {
    input: clickNode,
    output: out,
    passes: passCount,
    nodes: { clickNode, tuningNode, stages, out },
    reset,
    applySettings,
  };
}
