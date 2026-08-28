import { buildLameGraph } from "../lame/src/audio/graph.js?v=20260827.43";
import { MASTER_PRESETS } from "../lame/src/presets.js?v=20260827.33";

const presetIds = [
  "apartment-tv-next-door",
  "motel-answering-machine",
  "webcam-call-2008",
  "mall-security-archive",
  "radio-in-the-vent",
  "bootleg-game-cutscene",
  "scratched-disc-over-discord",
  "hospital-lockdown-pa",
  "ghost-vhs-screening",
  "pirate-station-cassette",
];
const sampleRate = 48000;
const seconds = 2.4;
const frames = Math.round(sampleRate * seconds);
const seed = 0x5241434b;

function makeFixture(context, silence) {
  const buffer = context.createBuffer(1, frames, sampleRate);
  if (silence) return buffer;
  const data = buffer.getChannelData(0);
  let state = 0x19ac7e31;
  for (let i = 0; i < data.length; i++) {
    const t = i / sampleRate;
    state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
    const noise = (state / 0xffffffff) * 2 - 1;
    const phrase = i % Math.round(sampleRate * 0.8);
    const voiced = phrase < sampleRate * 0.56;
    const syllable = voiced ? 0.35 + 0.65 * Math.pow(Math.max(0, Math.sin(Math.PI * phrase / (sampleRate * 0.56))), 0.5) : 0.008;
    const transient = i % 12000 < 12 ? 0.42 * Math.exp(-(i % 12000) / 3) : 0;
    data[i] = syllable * (
      0.28 * Math.sin(2 * Math.PI * 123 * t) +
      0.17 * Math.sin(2 * Math.PI * 369 * t) +
      0.1 * Math.sin(2 * Math.PI * 1130 * t) +
      noise * 0.025
    ) + transient;
  }
  return buffer;
}

function activeModules(preset) {
  const byType = new Map();
  for (const [type, config] of Object.entries(preset.modules || {})) {
    if (!config?.enabled) continue;
    byType.set(type, {
      instanceId: byType.size + 1,
      type,
      enabled: true,
      wet: config.wet ?? 1,
      params: { ...(config.params || {}) },
    });
  }
  const ordered = [];
  for (const type of preset.order || []) {
    const module = byType.get(type);
    if (module) {
      ordered.push(module);
      byType.delete(type);
    }
  }
  ordered.push(...byType.values());
  return ordered;
}

async function render(preset, silence = false) {
  const context = new OfflineAudioContext(1, frames, sampleRate);
  const modules = activeModules(preset);
  const graph = await buildLameGraph(context, { seed, modules, stereo: false, withMaster: true });
  graph.input.gain.setValueAtTime(preset.master?.inputTrim ?? 0.5, 0);
  for (const module of modules) {
    const wrapper = graph.modules.get(module.instanceId);
    wrapper.applySettings(module.params, { time: 0, ramp: 0 });
    wrapper.setWetEnabled({ wet: module.wet, enabled: true }, { time: 0, ramp: 0 });
  }
  graph.applyMaster(preset.master || {}, { time: 0, ramp: 0 });
  graph.resetAll();
  graph.output.connect(context.destination);
  const source = new AudioBufferSourceNode(context, { buffer: makeFixture(context, silence) });
  source.connect(graph.input);
  source.start();
  return context.startRendering();
}

function metrics(buffer, ceiling) {
  const data = buffer.getChannelData(0);
  let energy = 0;
  let peak = 0;
  let nonFinite = 0;
  let clipped = 0;
  for (const sample of data) {
    if (!Number.isFinite(sample)) {
      nonFinite++;
      continue;
    }
    const amplitude = Math.abs(sample);
    energy += sample * sample;
    peak = Math.max(peak, amplitude);
    if (amplitude >= ceiling - 0.00005) clipped++;
  }
  return {
    peak,
    rms: Math.sqrt(energy / data.length),
    nonFinite,
    ceilingSafe: peak <= ceiling + 0.0001,
    clippedPercent: clipped * 100 / data.length,
  };
}

const results = {};
for (const id of presetIds) {
  const preset = MASTER_PRESETS.find((candidate) => candidate.id === id);
  if (!preset) {
    results[id] = { missing: true };
    continue;
  }
  const ceiling = preset.master?.ceiling ?? 0.92;
  results[id] = {
    modules: activeModules(preset).map((module) => module.type),
    signal: metrics(await render(preset), ceiling),
    silence: metrics(await render(preset, true), ceiling),
  };
  document.querySelector("#results").textContent = JSON.stringify(results, null, 2);
}
document.body.dataset.complete = "true";
