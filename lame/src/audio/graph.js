import { buildTransmissionGraph } from "../../../src/audio/graph.js?v=20260827.4";
import { buildOcclusionGraph } from "../../../occlusion-engine/src/audio/graph.js?v=20260828.20";
import { buildTapeGraph } from "../../../tape-engine/src/audio/graph.js?v=20260827.21";
import { buildTelevisionGraph } from "../../../television-engine/src/audio/graph.js?v=20260827.2";
import { buildCartridgeGraph } from "../../../cartridge-engine/src/audio/graph.js?v=20260827.26";
import { buildCommsGraph } from "../../../comms-engine/src/audio/graph.js?v=20260827.26";
import { buildConferenceGraph } from "../../../conference-engine/src/audio/graph.js?v=20260827.24";
import { buildCdGraph } from "../../../cd-engine/src/audio/graph.js?v=20260827.6";
import { buildCamcorderGraph } from "../../../camcorder-engine/src/audio/graph.js?v=20260828.28";

const MASTER_NOISE_WORKLET_URL = new URL("./master-noise-reducer-processor.js", import.meta.url);
const MASTER_WORKLET_CONTEXTS = new WeakSet();
export const MASTER_EQ_FREQUENCIES = [31, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000];

export async function ensureMasterWorklets(ctx) {
  if (MASTER_WORKLET_CONTEXTS.has(ctx)) return;
  if (!ctx?.audioWorklet) throw new Error("This browser does not support the mastering noise-reduction worklet.");
  await ctx.audioWorklet.addModule(MASTER_NOISE_WORKLET_URL.href);
  MASTER_WORKLET_CONTEXTS.add(ctx);
}

function now(ctx) {
  return ctx.currentTime;
}

function mixSeed(base, salt) {
  let x = (base ^ salt) >>> 0;
  x = Math.imul(x ^ (x >>> 16), 0x7feb352d) >>> 0;
  x = Math.imul(x ^ (x >>> 15), 0x846ca68b) >>> 0;
  return (x ^ (x >>> 16)) >>> 0;
}

function eqPowGains(wet01) {
  const w = Math.min(1, Math.max(0, wet01));
  const a = w * Math.PI * 0.5;
  return { dry: Math.cos(a), wet: Math.sin(a) };
}

function makeSoftClipCurve(k = 2.5, n = 2048) {
  const curve = new Float32Array(n);
  const dk = Math.max(0.1, k);
  const norm = Math.tanh(dk) || 1;
  for (let i = 0; i < n; i++) {
    const x = (i / (n - 1)) * 2 - 1;
    curve[i] = Math.tanh(dk * x) / norm;
  }
  return curve;
}

const IDENTITY_CURVE = (() => {
  const n = 2048;
  const c = new Float32Array(n);
  for (let i = 0; i < n; i++) c[i] = (i / (n - 1)) * 2 - 1;
  return c;
})();

function dbFromLin(lin) {
  const x = Math.max(1e-6, Math.min(1, lin));
  return 20 * Math.log10(x);
}

async function buildModuleLane(ctx, module, { seed, tuningEdges, tuningSample }) {
  const type = module.type;
  if (type === "occlusion") return await buildOcclusionGraph(ctx, { seed });
  if (type === "transmission") {
    return await buildTransmissionGraph(ctx, {
      seed,
      passes: module.params?.passes ?? 1,
      tuningEdges,
      tuningSample,
    });
  }
  if (type === "tape") return await buildTapeGraph(ctx, { seed });
  if (type === "television") return await buildTelevisionGraph(ctx, { seed });
  if (type === "cartridge") return await buildCartridgeGraph(ctx, { seed });
  if (type === "comms") return await buildCommsGraph(ctx, { seed });
  if (type === "conference") return await buildConferenceGraph(ctx, { seed });
  if (type === "cd") return await buildCdGraph(ctx, { seed });
  if (type === "camcorder") return await buildCamcorderGraph(ctx, { seed });
  throw new Error(`Unknown module type: ${type}`);
}

function wrapWetDry(ctx, laneInput, moduleGraph) {
  const dryGain = new GainNode(ctx, { gain: 1 });
  const wetGain = new GainNode(ctx, { gain: 0 });
  const sum = new GainNode(ctx, { gain: 1 });
  const latencySeconds = Math.max(0, Number(moduleGraph.latencySeconds) || 0);
  const dryDelay = latencySeconds > 0
    ? new DelayNode(ctx, { maxDelayTime: Math.max(0.05, latencySeconds), delayTime: latencySeconds })
    : null;

  if (dryDelay) {
    laneInput.connect(dryDelay);
    dryDelay.connect(dryGain);
  } else {
    laneInput.connect(dryGain);
  }
  dryGain.connect(sum);

  laneInput.connect(moduleGraph.input);
  moduleGraph.output.connect(wetGain);
  wetGain.connect(sum);

  return { input: laneInput, output: sum, dryGain, wetGain, dryDelay, mixLaw: moduleGraph.mixLaw || "equal-power" };
}

export function buildMasterLane(ctx) {
  const input = new GainNode(ctx, { gain: 1 });
  const masterGain = new GainNode(ctx, { gain: 1 });
  const hp1 = new BiquadFilterNode(ctx, { type: "highpass", frequency: 20, Q: 0.707 });
  const hp2 = new BiquadFilterNode(ctx, { type: "highpass", frequency: 20, Q: 0.707 });
  const lp1 = new BiquadFilterNode(ctx, { type: "lowpass", frequency: 20000, Q: 0.707 });
  const lp2 = new BiquadFilterNode(ctx, { type: "lowpass", frequency: 20000, Q: 0.707 });
  const graphicEqNodes = MASTER_EQ_FREQUENCIES.map((frequency, index) => new BiquadFilterNode(ctx, {
    type: index === 0 ? "lowshelf" : index === MASTER_EQ_FREQUENCIES.length - 1 ? "highshelf" : "peaking",
    frequency,
    Q: index === 0 || index === MASTER_EQ_FREQUENCIES.length - 1 ? 0.707 : 1.35,
    gain: 0,
  }));
  const noiseReducer = new AudioWorkletNode(ctx, "master-noise-reducer", { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [1] });

  const compNode = new DynamicsCompressorNode(ctx, { threshold: -16, knee: 24, ratio: 3, attack: 0.01, release: 0.16 });
  const limiterNode = new DynamicsCompressorNode(ctx, { threshold: -1.2, knee: 0, ratio: 20, attack: 0.003, release: 0.09 });
  const colorShaper = new WaveShaperNode(ctx, { curve: IDENTITY_CURVE, oversample: "4x" });
  const colorGain = new GainNode(ctx, { gain: 1 });
  const spaceDry = new GainNode(ctx, { gain: 1 });
  const delayNode = new DelayNode(ctx, { maxDelayTime: 1, delayTime: 0.18 });
  const delayDamping = new BiquadFilterNode(ctx, { type: "lowpass", frequency: 8000, Q: 0.707 });
  const delayFeedback = new GainNode(ctx, { gain: 0.18 });
  const delayWet = new GainNode(ctx, { gain: 0 });
  const reverbPreDelay = new DelayNode(ctx, { maxDelayTime: 0.25, delayTime: 0.018 });
  const reverbNode = new ConvolverNode(ctx);
  const reverbDamping = new BiquadFilterNode(ctx, { type: "lowpass", frequency: 6500, Q: 0.707 });
  const reverbWet = new GainNode(ctx, { gain: 0 });
  const spaceSum = new GainNode(ctx, { gain: 1 });
  const shaper = new WaveShaperNode(ctx, { curve: makeSoftClipCurve(2.7), oversample: "4x" });
  const ceilingGain = new GainNode(ctx, { gain: 1 });
  const output = new GainNode(ctx, { gain: 1 });

  const makeImpulse = (seconds) => {
    const duration = Math.max(0.3, Math.min(8, seconds));
    const rate = ctx.sampleRate || 48000;
    const impulse = ctx.createBuffer(1, Math.max(1, Math.floor(rate * duration)), rate);
    const data = impulse.getChannelData(0);
    let seed = 0x5eeda11;
    for (let i = 0; i < data.length; i++) {
      seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
      const noise = (seed / 0xffffffff) * 2 - 1;
      data[i] = noise * Math.pow(1 - i / data.length, 3.2);
    }
    return impulse;
  };
  let currentDecay = 1.35;
  reverbNode.buffer = makeImpulse(currentDecay);

  input.connect(masterGain);
  masterGain.connect(hp1);
  hp1.connect(hp2);
  let eqHead = hp2;
  for (const node of graphicEqNodes) { eqHead.connect(node); eqHead = node; }
  eqHead.connect(lp1);
  lp1.connect(lp2);
  lp2.connect(noiseReducer);
  noiseReducer.connect(compNode);
  compNode.connect(colorShaper);
  colorShaper.connect(colorGain);
  colorGain.connect(spaceDry);
  spaceDry.connect(spaceSum);
  colorGain.connect(delayNode);
  delayNode.connect(delayDamping);
  delayDamping.connect(delayWet);
  delayWet.connect(spaceSum);
  delayDamping.connect(delayFeedback);
  delayFeedback.connect(delayNode);
  colorGain.connect(reverbPreDelay);
  reverbPreDelay.connect(reverbNode);
  reverbNode.connect(reverbDamping);
  reverbDamping.connect(reverbWet);
  reverbWet.connect(spaceSum);
  spaceSum.connect(limiterNode);
  limiterNode.connect(shaper);
  shaper.connect(ceilingGain);
  ceilingGain.connect(output);

  function applySettings(s, { time = now(ctx), ramp = 0.02 } = {}) {
    const t1 = time + ramp;
    const lerp = (a, b, t) => a + (b - a) * t;
    const rampParam = (param, value) => {
      param.cancelScheduledValues(time);
      param.setValueAtTime(param.value, time);
      param.linearRampToValueAtTime(value, t1);
    };
    rampParam(masterGain.gain, Math.max(0, Math.min(2.5, s.masterGain ?? 1)));

    const nyq = (ctx.sampleRate || 48000) * 0.5;
    const clampHz = (hz, lo, hi) => Math.max(lo, Math.min(hi, Number.isFinite(hz) ? hz : lo));
    const hpHz = clampHz(s.masterHpHz ?? 20, 10, nyq * 0.95);
    const lpHz = clampHz(Math.max(s.masterLpHz ?? 20000, hpHz + 30), 40, nyq * 0.95);
    for (const node of [hp1, hp2]) rampParam(node.frequency, hpHz);
    for (const node of [lp1, lp2]) rampParam(node.frequency, lpHz);
    graphicEqNodes.forEach((node, index) => rampParam(node.gain, Math.max(-12, Math.min(12, Number(s.masterEqBands?.[String(MASTER_EQ_FREQUENCIES[index])] || 0)))));

    const noiseSettings = [
      ["thresholdDb", Math.max(-90, Math.min(-10, s.masterNoiseThreshold ?? -55))],
      ["reductionDb", Math.max(0, Math.min(48, s.masterNoiseReductionDb ?? 12))],
      ["attackMs", Math.max(1, Math.min(200, s.masterNoiseAttack ?? 12))],
      ["releaseMs", Math.max(10, Math.min(2000, s.masterNoiseRelease ?? 220))],
      ["mix", Math.max(0, Math.min(1, s.masterNoiseMix ?? 0))],
      ["learn", s.masterNoiseLearn ? 1 : 0],
    ];
    for (const [name, value] of noiseSettings) rampParam(noiseReducer.parameters.get(name), value);

    const compAmt = Math.max(0, Math.min(1, s.masterComp ?? 0));
    const compEnabled = compAmt > 0.0001;
    compNode.threshold.setValueAtTime(compEnabled ? lerp(-10, -34, compAmt) : 0, time);
    compNode.ratio.setValueAtTime(compEnabled ? lerp(2, 9, compAmt) : 1, time);
    compNode.knee.setValueAtTime(compEnabled ? lerp(18, 34, compAmt) : 0, time);
    compNode.attack.setValueAtTime(compEnabled ? lerp(0.03, 0.006, compAmt) : 0.003, time);
    compNode.release.setValueAtTime(compEnabled ? lerp(0.26, 0.09, compAmt) : 0.05, time);

    const saturation = Math.max(0, Math.min(1, s.masterSaturation ?? 0));
    colorShaper.curve = saturation > 0.0001 ? makeSoftClipCurve(1.25 + saturation * 4.2) : IDENTITY_CURVE;
    rampParam(colorGain.gain, 1 - saturation * 0.14);
    const delayMix = Math.max(0, Math.min(1, s.masterDelayMix ?? 0));
    const reverbMix = Math.max(0, Math.min(1, s.masterReverbMix ?? 0));
    rampParam(delayNode.delayTime, Math.max(0.01, Math.min(1, (s.masterDelayTime ?? 180) / 1000)));
    rampParam(delayFeedback.gain, Math.max(0, Math.min(0.85, s.masterDelayFeedback ?? 0.18)));
    rampParam(delayDamping.frequency, clampHz(s.masterDelayDamping ?? 8000, 200, nyq * 0.95));
    rampParam(delayWet.gain, delayMix * 0.65);
    rampParam(reverbPreDelay.delayTime, Math.max(0, Math.min(0.2, (s.masterReverbPreDelay ?? 18) / 1000)));
    rampParam(reverbDamping.frequency, clampHz(s.masterReverbDamping ?? 6500, 200, nyq * 0.95));
    rampParam(reverbWet.gain, reverbMix * 0.58);
    rampParam(spaceDry.gain, Math.max(0.62, 1 - delayMix * 0.2 - reverbMix * 0.16));
    const nextDecay = Math.max(0.3, Math.min(8, s.masterReverbDecay ?? 1.35));
    if (Math.abs(nextDecay - currentDecay) >= 0.075) {
      currentDecay = nextDecay;
      reverbNode.buffer = makeImpulse(currentDecay);
    }

    const ceiling = Math.max(0.05, Math.min(1, s.ceiling ?? 0.92));
    const limAmt = Math.max(0, Math.min(1, s.limiter ?? 0.6));
    const limEnabled = limAmt > 0.0001;
    limiterNode.threshold.setValueAtTime(limEnabled ? dbFromLin(ceiling) : 0, time);
    limiterNode.ratio.setValueAtTime(limEnabled ? lerp(4, 20, limAmt) : 1, time);
    limiterNode.knee.setValueAtTime(limEnabled ? lerp(18, 0, limAmt) : 0, time);
    limiterNode.attack.setValueAtTime(limEnabled ? lerp(0.02, 0.003, limAmt) : 0.003, time);
    limiterNode.release.setValueAtTime(limEnabled ? lerp(0.18, 0.09, limAmt) : 0.05, time);
    const soft = Boolean(s.softClip);
    shaper.curve = soft ? makeSoftClipCurve(2.7 + limAmt * 1.2) : IDENTITY_CURVE;
    ceilingGain.gain.setValueAtTime(soft ? ceiling : 1, time);
  }

  return {
    input,
    output,
    applySettings,
    setNoiseFloorListener(listener) {
      noiseReducer.port.onmessage = (event) => {
        if (event.data?.type === "noiseFloor" && typeof listener === "function") listener(event.data.value);
      };
    },
    nodes: { input, masterGain, hp1, hp2, lp1, lp2, graphicEqNodes, noiseReducer, compNode, colorShaper, delayNode, delayDamping, reverbPreDelay, reverbNode, reverbDamping, limiterNode, shaper, ceilingGain, output },
  };
}

export async function buildLameGraph(ctx, { seed, modules, stereo = false, tuningEdges = null, tuningSample = null, withMaster = true } = {}) {
  if (withMaster) await ensureMasterWorklets(ctx);
  const chanCount = stereo ? 2 : 1;
  const input = new GainNode(ctx, { gain: 1, channelCount: chanCount });

  let laneStarts = [input];
  let splitter = null;
  if (chanCount === 2) {
    splitter = new ChannelSplitterNode(ctx, { numberOfOutputs: 2 });
    input.connect(splitter);
    const inL = new GainNode(ctx, { gain: 1 });
    const inR = new GainNode(ctx, { gain: 1 });
    splitter.connect(inL, 0);
    splitter.connect(inR, 1);
    laneStarts = [inL, inR];
  }

  const moduleMap = new Map(); // instanceId -> wrapper
  const lanes = laneStarts.map((n) => ({ head: n }));

  for (const m of modules) {
    const instanceId = m.instanceId >>> 0;
    const typeSalt =
      m.type === "occlusion"
        ? 0x0cc11510
        : m.type === "transmission"
        ? 0x7a11a11a
        : m.type === "comms"
          ? 0xc0de5001
          : m.type === "conference"
            ? 0xc0ffee01
          : m.type === "tape"
            ? 0x07a9e001
            : m.type === "television"
              ? 0x7e1e5150
            : m.type === "cartridge"
              ? 0xca7271d6
              : m.type === "cd"
                ? 0xcd0000cd
                : 0xcacc0dde;
    const laneGraphs = [];
    const laneWraps = [];

    for (let lane = 0; lane < lanes.length; lane++) {
      // Optical failures belong to one disc/read head, so keep CD event timing
      // and Random conceal choices coherent across stereo lanes.
      const laneSalt = m.type === "cd" ? 0 : lane * 0x51ed270b;
      const laneSeed = mixSeed(seed >>> 0, typeSalt ^ Math.imul(instanceId + 1, 0x9e3779b9) ^ laneSalt);
      const g = await buildModuleLane(ctx, m, { seed: laneSeed, tuningEdges, tuningSample });
      const w = wrapWetDry(ctx, lanes[lane].head, g);
      lanes[lane].head = w.output;
      laneGraphs.push({ graph: g, seed: laneSeed });
      laneWraps.push(w);
    }

    moduleMap.set(instanceId, {
      module: m,
      laneGraphs,
      laneWraps,
      setWetEnabled({ wet = 1, enabled = true }, { time = now(ctx), ramp = 0.02 } = {}) {
        const v = enabled ? Math.max(0, Math.min(1, wet)) : 0;
        const t1 = time + ramp;
        for (const w of laneWraps) {
          const mix = w.mixLaw === "linear" ? { dry: 1 - v, wet: v } : eqPowGains(v);
          w.dryGain.gain.cancelScheduledValues(time);
          w.dryGain.gain.setValueAtTime(w.dryGain.gain.value, time);
          w.dryGain.gain.linearRampToValueAtTime(mix.dry, t1);
          w.wetGain.gain.cancelScheduledValues(time);
          w.wetGain.gain.setValueAtTime(w.wetGain.gain.value, time);
          w.wetGain.gain.linearRampToValueAtTime(mix.wet, t1);
        }
      },
      applySettings(settings, { time = now(ctx), ramp = 0.02 } = {}) {
        for (const lg of laneGraphs) lg.graph.applySettings(settings, { time, ramp });
      },
      reset() {
        for (const lg of laneGraphs) lg.graph.reset(lg.seed >>> 0);
      },
      triggerDamage(strength = 1) {
        for (const lg of laneGraphs) lg.graph.triggerDamage?.(strength);
      },
      triggerSkip(strength = 1) {
        for (const lg of laneGraphs) lg.graph.triggerSkip?.(strength);
      },
      setTuningSample(sample) {
        for (const lg of laneGraphs) {
          const tnode = lg.graph?.nodes?.tuningNode;
          if (tnode && sample) {
            tnode.port.postMessage({ type: "setSample", sampleRate: sample.sampleRate, data: sample.data });
          }
        }
      },
    });
  }

  const masters = [];
  if (withMaster) {
    for (let lane = 0; lane < lanes.length; lane++) {
      const master = buildMasterLane(ctx);
      lanes[lane].head.connect(master.input);
      lanes[lane].head = master.output;
      masters.push(master);
    }
  }

  let output = null;
  if (chanCount === 2) {
    const merger = new ChannelMergerNode(ctx, { numberOfInputs: 2 });
    lanes[0].head.connect(merger, 0, 0);
    lanes[1].head.connect(merger, 0, 1);
    output = merger;
  } else {
    output = lanes[0].head;
  }

  function resetAll() {
    for (const w of moduleMap.values()) w.reset();
  }

  function applyMaster(settings, opts) {
    for (const m of masters) m.applySettings(settings, opts);
  }

  return { input, output, stereo, nodes: { input, splitter }, modules: moduleMap, masters, applyMaster, resetAll };
}
