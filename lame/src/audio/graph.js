import { buildTransmissionGraph } from "../../../src/audio/graph.js";
import { buildOcclusionGraph } from "../../../occlusion-engine/src/audio/graph.js";
import { buildTapeGraph } from "../../../tape-engine/src/audio/graph.js";
import { buildTelevisionGraph } from "../../../television-engine/src/audio/graph.js";
import { buildCartridgeGraph } from "../../../cartridge-engine/src/audio/graph.js";
import { buildCommsGraph } from "../../../comms-engine/src/audio/graph.js";
import { buildConferenceGraph } from "../../../conference-engine/src/audio/graph.js";
import { buildCdGraph } from "../../../cd-engine/src/audio/graph.js";
import { buildCamcorderGraph } from "../../../camcorder-engine/src/audio/graph.js";

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

  laneInput.connect(dryGain);
  dryGain.connect(sum);

  laneInput.connect(moduleGraph.input);
  moduleGraph.output.connect(wetGain);
  wetGain.connect(sum);

  return { input: laneInput, output: sum, dryGain, wetGain };
}

export function buildMasterLane(ctx) {
  const input = new GainNode(ctx, { gain: 1 });
  const masterGain = new GainNode(ctx, { gain: 1 });

  // Master EQ (stacked for a steeper slope).
  const hp1 = new BiquadFilterNode(ctx, { type: "highpass", frequency: 20, Q: 0.707 });
  const hp2 = new BiquadFilterNode(ctx, { type: "highpass", frequency: 20, Q: 0.707 });
  const lp1 = new BiquadFilterNode(ctx, { type: "lowpass", frequency: 20000, Q: 0.707 });
  const lp2 = new BiquadFilterNode(ctx, { type: "lowpass", frequency: 20000, Q: 0.707 });

  // Master compression (serial). NOTE: avoid parallel mixing with a dry path here, because
  // DynamicsCompressorNode may introduce internal latency/lookahead; mixing can cause comb filtering.
  const compNode = new DynamicsCompressorNode(ctx, {
    threshold: -16,
    knee: 24,
    ratio: 3,
    attack: 0.01,
    release: 0.16,
  });

  const limiterNode = new DynamicsCompressorNode(ctx, {
    threshold: -1.2,
    knee: 0,
    ratio: 20,
    attack: 0.003,
    release: 0.09,
  });

  const shaper = new WaveShaperNode(ctx, { curve: makeSoftClipCurve(2.7), oversample: "4x" });
  const ceilingGain = new GainNode(ctx, { gain: 1 });
  const output = new GainNode(ctx, { gain: 1 });

  input.connect(masterGain);

  masterGain.connect(hp1);
  hp1.connect(hp2);
  hp2.connect(lp1);
  lp1.connect(lp2);
  lp2.connect(compNode);
  compNode.connect(limiterNode);
  limiterNode.connect(shaper);
  shaper.connect(ceilingGain);
  ceilingGain.connect(output);

  function applySettings(s, { time = now(ctx), ramp = 0.02 } = {}) {
    const t1 = time + ramp;
    const gain = Math.max(0, Math.min(2.5, s.masterGain ?? 1));
    masterGain.gain.cancelScheduledValues(time);
    masterGain.gain.setValueAtTime(masterGain.gain.value, time);
    masterGain.gain.linearRampToValueAtTime(gain, t1);

    const nyq = (ctx.sampleRate || 48000) * 0.5;
    const clampHz = (hz, lo, hi) => Math.max(lo, Math.min(hi, Number.isFinite(hz) ? hz : lo));
    const hpHz = clampHz(s.masterHpHz ?? 20, 10, nyq * 0.95);
    const lpHz0 = clampHz(s.masterLpHz ?? 20000, 40, nyq * 0.95);
    const lpHz = clampHz(Math.max(lpHz0, hpHz + 30), 40, nyq * 0.95);

    for (const n of [hp1, hp2]) {
      n.frequency.cancelScheduledValues(time);
      n.frequency.setValueAtTime(n.frequency.value, time);
      n.frequency.linearRampToValueAtTime(hpHz, t1);
    }
    for (const n of [lp1, lp2]) {
      n.frequency.cancelScheduledValues(time);
      n.frequency.setValueAtTime(n.frequency.value, time);
      n.frequency.linearRampToValueAtTime(lpHz, t1);
    }

    const compAmt = Math.max(0, Math.min(1, s.masterComp ?? 0));
    // Map 0..1 -> gentle to firm bus compression.
    const lerp = (a, b, t) => a + (b - a) * t;
    const compEnabled = compAmt > 0.0001;
    compNode.threshold.setValueAtTime(compEnabled ? lerp(-10, -34, compAmt) : 0, time);
    compNode.ratio.setValueAtTime(compEnabled ? lerp(2, 9, compAmt) : 1, time);
    compNode.knee.setValueAtTime(compEnabled ? lerp(18, 34, compAmt) : 0, time);
    compNode.attack.setValueAtTime(compEnabled ? lerp(0.03, 0.006, compAmt) : 0.003, time);
    compNode.release.setValueAtTime(compEnabled ? lerp(0.26, 0.09, compAmt) : 0.05, time);

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
    nodes: {
      input,
      masterGain,
      hp1,
      hp2,
      lp1,
      lp2,
      compNode,
      limiterNode,
      shaper,
      ceilingGain,
      output,
    },
  };
}

export async function buildLameGraph(ctx, { seed, modules, stereo = false, tuningEdges = null, tuningSample = null, withMaster = true } = {}) {
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
      const laneSeed = mixSeed(seed >>> 0, typeSalt ^ Math.imul(instanceId + 1, 0x9e3779b9) ^ (lane * 0x51ed270b));
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
        const mix = eqPowGains(v);
        const t1 = time + ramp;
        for (const w of laneWraps) {
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
