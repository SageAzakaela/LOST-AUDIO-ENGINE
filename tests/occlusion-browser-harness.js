import { buildOcclusionGraph } from "../occlusion-engine/src/audio/graph.js?v=20260828.20";

const sampleRate = 48000;
const seconds = 3;
const frames = sampleRate * seconds;

function makeExcitation(ctx) {
  const buffer = ctx.createBuffer(1, frames, sampleRate);
  const data = buffer.getChannelData(0);
  let seed = 0x51a7e;
  for (let i = 0; i < data.length; i++) {
    const t = i / sampleRate;
    const phase = i % 12000;
    const transient = phase < 1800 ? Math.exp(-phase / 280) : 0;
    seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
    const noise = (seed / 0xffffffff) * 2 - 1;
    const tone = Math.sin(2 * Math.PI * 113 * t) * 0.16 + Math.sin(2 * Math.PI * 487 * t) * 0.1;
    data[i] = Math.max(-0.92, Math.min(0.92, tone + transient * (0.62 + noise * 0.18)));
  }
  return buffer;
}

const base = {
  distance: 0.3, wall: 0.5, material: "drywall", construction: "stud",
  sourceRoom: 0.3, listenerRoom: 0.3,
  hpHz: 35, lpHz: 16000, bumpHz: 350, bumpDb: 0, bumpQ: 0.95,
  dipHz: 1550, dipDb: 0, dipQ: 1.1,
  resonance: 0, cavity: 0, rattle: 0, looseness: 0.8, smear: 0,
  leak: 0, leakTone: 0.5, roomMix: 0, predelayMs: 0, roomSize: 0.3, damp: 0.5, outGain: 1,
};

async function render(overrides = {}, { macroOnly = false } = {}) {
  const context = new OfflineAudioContext(1, frames, sampleRate);
  const graph = await buildOcclusionGraph(context, { seed: 0x1234abcd });
  graph.output.connect(context.destination);
  graph.applySettings(macroOnly ? overrides : { ...base, ...overrides }, { time: 0, ramp: 0.001 });
  graph.reset();
  const source = new AudioBufferSourceNode(context, { buffer: makeExcitation(context) });
  source.connect(graph.input);
  source.start(0.01);
  const rendered = await context.startRendering();
  return new Float32Array(rendered.getChannelData(0));
}

function textureMetrics(signal) {
  let energy = 0;
  let derivativeEnergy = 0;
  let peak = 0;
  for (let i = 0; i < signal.length; i++) {
    const x = signal[i];
    energy += x * x;
    if (i > 0) {
      const d = x - signal[i - 1];
      derivativeEnergy += d * d;
    }
    peak = Math.max(peak, Math.abs(x));
  }
  const rms = Math.sqrt(energy / signal.length);
  const derivativeRms = Math.sqrt(derivativeEnergy / Math.max(1, signal.length - 1));
  return { rms, derivativeRms, brightnessProxy: derivativeRms / Math.max(1e-9, rms), peak };
}

function metrics(reference, candidate) {
  let refEnergy = 0;
  let candidateEnergy = 0;
  let diffEnergy = 0;
  let dot = 0;
  let peak = 0;
  for (let i = 0; i < reference.length; i++) {
    const a = reference[i];
    const b = candidate[i];
    const d = b - a;
    refEnergy += a * a;
    candidateEnergy += b * b;
    diffEnergy += d * d;
    dot += a * b;
    peak = Math.max(peak, Math.abs(b));
  }
  const n = reference.length;
  const refRms = Math.sqrt(refEnergy / n);
  const candidateRms = Math.sqrt(candidateEnergy / n);
  const diffRms = Math.sqrt(diffEnergy / n);
  return {
    refRms, candidateRms, diffRms,
    differencePercent: refRms > 0 ? (diffRms / refRms) * 100 : 0,
    correlation: dot / Math.max(1e-12, Math.sqrt(refEnergy * candidateEnergy)),
    peak,
  };
}

const reference = await render();
const cases = {
  resonance: { resonance: 1 },
  cavity: { cavity: 1, resonance: 0.65 },
  rattle: { rattle: 1, looseness: 1 },
  smear: { smear: 1 },
  leak: { leak: 0.6, leakTone: 0.8 },
  sourceRoom: { sourceRoom: 1, listenerRoom: 0, roomMix: 0.65, roomSize: 0.5 },
  listenerRoom: { sourceRoom: 0, listenerRoom: 1, roomMix: 0.65, roomSize: 1, predelayMs: 18 },
};

const results = {};
for (const [name, settings] of Object.entries(cases)) results[name] = metrics(reference, await render(settings));
const materialBase = await render({ resonance: 0.75, cavity: 0.55, rattle: 0.35, looseness: 0.7, smear: 0.55 });
for (const material of ["brick", "wood", "glass", "metal", "concrete"]) {
  results[`material:${material}`] = metrics(materialBase, await render({ material, resonance: 0.75, cavity: 0.55, rattle: 0.35, looseness: 0.7, smear: 0.55 }));
}

const macroReference = await render({ distance: 0.25, wall: 0, material: "drywall", construction: "stud", sourceRoom: 0.25, listenerRoom: 0.25 }, { macroOnly: true });
results["macro:drywall-open"] = textureMetrics(macroReference);
for (const [name, material, construction] of [
  ["drywall-stud", "drywall", "stud"],
  ["brick-solid", "brick", "solid"],
  ["wood-hollow", "wood", "hollow"],
  ["glass-panel", "glass", "panel"],
  ["metal-loose", "metal", "loose"],
  ["concrete-solid", "concrete", "solid"],
]) {
  const rendered = await render({ distance: 0.48, wall: 0.78, material, construction, sourceRoom: 0.3, listenerRoom: 0.42 }, { macroOnly: true });
  results[`macro:${name}`] = { ...metrics(macroReference, rendered), ...textureMetrics(rendered) };
}

document.querySelector("#results").textContent = JSON.stringify(results, null, 2);
document.body.dataset.complete = "true";
