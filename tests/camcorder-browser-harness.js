import { buildCamcorderGraph, defaultSettings } from "../camcorder-engine/src/audio/graph.js?v=20260827.27";
import { PRESETS } from "../camcorder-engine/src/presets.js?v=20260827.27";

const sampleRate = 48000;
const seconds = 2;
const frames = sampleRate * seconds;
const seed = 0x43414d51;

function makeInput(context, silence = false) {
  const buffer = context.createBuffer(1, frames, sampleRate);
  const data = buffer.getChannelData(0);
  if (silence) return buffer;
  let noiseSeed = 0x91e10da5;
  for (let i = 0; i < frames; i++) {
    const t = i / sampleRate;
    noiseSeed = (Math.imul(noiseSeed, 1664525) + 1013904223) >>> 0;
    const noise = (noiseSeed / 0xffffffff) * 2 - 1;
    const phrase = i % 24000;
    const envelope = phrase < 13000 ? 0.65 : phrase < 18000 ? 0.12 : 0.015;
    data[i] = envelope * (
      Math.sin(2 * Math.PI * 117 * t) * 0.31 +
      Math.sin(2 * Math.PI * 431 * t) * 0.24 +
      Math.sin(2 * Math.PI * 1733 * t) * 0.18 +
      noise * 0.04
    );
  }
  return buffer;
}

async function render(settings, silence = false) {
  const context = new OfflineAudioContext(1, frames, sampleRate);
  const graph = await buildCamcorderGraph(context, { seed });
  graph.applySettings(settings, { time: 0, ramp: 0 });
  graph.reset(seed);
  graph.output.connect(context.destination);
  const source = new AudioBufferSourceNode(context, { buffer: makeInput(context, silence) });
  source.connect(graph.input);
  source.start(0);
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

async function renderWindIsolation(corruption) {
  const context = new OfflineAudioContext(1, frames, sampleRate);
  const settings = {
    ...defaultSettings(),
    wind: true,
    windLevel: 1,
    corruption,
    movement: 0,
    agc: 0,
    agcAmt: 0,
    clip: 0,
    crush: 0,
    flutter: 0,
    drop: 0,
    chirp: 0,
    handling: 0,
    rub: 0,
    hiss: 0,
    motorBleed: 0,
    outGain: 1,
    ceiling: 0.92,
  };
  const graph = await buildCamcorderGraph(context, { seed });
  graph.applySettings(settings, { time: 0, ramp: 0 });
  graph.reset(seed);
  graph.output.connect(context.destination);

  const silent = new AudioBufferSourceNode(context, { buffer: makeInput(context, true) });
  silent.connect(graph.input);
  silent.start(0);

  const windBuffer = context.createBuffer(1, frames, sampleRate);
  const windData = windBuffer.getChannelData(0);
  let windSeed = 0x77aacc11;
  for (let i = 0; i < frames; i++) {
    windSeed = (Math.imul(windSeed, 1664525) + 1013904223) >>> 0;
    windData[i] = ((windSeed / 0xffffffff) * 2 - 1) * 0.45;
  }
  const windSource = new AudioBufferSourceNode(context, { buffer: windBuffer });
  windSource.connect(graph.wind);
  windSource.start(0);
  return context.startRendering();
}

function differenceRms(a, b) {
  const x = a.getChannelData(0);
  const y = b.getChannelData(0);
  let energy = 0;
  for (let i = 0; i < x.length; i++) {
    const d = x[i] - y[i];
    energy += d * d;
  }
  return Math.sqrt(energy / x.length);
}

const results = {};
for (const [name, preset] of Object.entries(PRESETS)) {
  const settings = { ...defaultSettings(), ...preset };
  const ceiling = settings.ceiling ?? 0.92;
  results[name] = {
    signal: metrics(await render(settings), ceiling),
    silence: metrics(await render(settings, true), ceiling),
    format: settings.format,
    micModel: settings.micModel,
  };
  document.querySelector("#results").textContent = JSON.stringify(results, null, 2);
}

const cleanWind = await renderWindIsolation(0);
const corruptWind = await renderWindIsolation(1);
results.__windIsolation = {
  clean: metrics(cleanWind, 0.92),
  corrupt: metrics(corruptWind, 0.92),
  differenceRms: differenceRms(cleanWind, corruptWind),
};
document.querySelector("#results").textContent = JSON.stringify(results, null, 2);

document.body.dataset.complete = "true";
