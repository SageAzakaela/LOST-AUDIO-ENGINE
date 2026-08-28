import { buildLameGraph } from "../lame/src/audio/graph.js?v=20260827.21";
import { PRESETS } from "../tape-engine/src/presets.js?v=20260827.21";

const sampleRate = 48000;
const seconds = 2;
const frames = sampleRate * seconds;
const frequencies = [83, 125, 250, 500, 1000, 2000, 4000, 8000];

const neutralTape = {
  quality: 1, age: 0, wow: 0, glitch: 0,
  hpHz: 10, lpHz: 22000, headBumpDb: 0, headBumpHz: 85,
  drive: 0, comp: 0, speed: 1, wowDepthMs: 0, flutterDepthMs: 0,
  hiss: 0, hum: 0, dropout: 0, dropoutMs: 38,
  ceiling: 1, outGain: 1, sfxEnable: false, sfxLevel: 0,
};

function makeSignal(ctx, impulse = false, silence = false) {
  const buffer = ctx.createBuffer(1, frames, sampleRate);
  const data = buffer.getChannelData(0);
  if (silence) return buffer;
  if (impulse) {
    data[Math.round(0.1 * sampleRate)] = 0.8;
    return buffer;
  }
  for (let i = 0; i < data.length; i++) {
    const t = i / sampleRate;
    let value = 0;
    for (const frequency of frequencies) value += Math.sin(2 * Math.PI * frequency * t);
    data[i] = value * (0.32 / frequencies.length);
  }
  return buffer;
}

async function render({ wet, impulse = false, silence = false, settings = neutralTape }) {
  const context = new OfflineAudioContext(1, frames, sampleRate);
  const module = { instanceId: 1, type: "tape", enabled: true, wet, params: settings };
  const graph = await buildLameGraph(context, { seed: 0x51a7e, modules: [module], withMaster: false });
  const wrapper = graph.modules.get(1);
  wrapper.applySettings(settings, { time: 0, ramp: 0.001 });
  wrapper.setWetEnabled({ wet, enabled: true }, { time: 0, ramp: 0.001 });
  graph.output.connect(context.destination);
  const source = new AudioBufferSourceNode(context, { buffer: makeSignal(context, impulse, silence) });
  source.connect(graph.input);
  source.start(0);
  const rendered = await context.startRendering();
  return new Float32Array(rendered.getChannelData(0));
}

function firstArrival(samples, threshold = 1e-4) {
  for (let i = 0; i < samples.length; i++) if (Math.abs(samples[i]) >= threshold) return i;
  return -1;
}

function toneLevel(samples, frequency) {
  const start = Math.round(0.35 * sampleRate);
  const end = Math.round(1.85 * sampleRate);
  let sin = 0;
  let cos = 0;
  for (let i = start; i < end; i++) {
    const phase = 2 * Math.PI * frequency * (i / sampleRate);
    sin += samples[i] * Math.sin(phase);
    cos += samples[i] * Math.cos(phase);
  }
  return (2 / (end - start)) * Math.hypot(sin, cos);
}

function signalMetrics(samples) {
  const start = Math.round(0.1 * sampleRate);
  let energy = 0;
  let mean = 0;
  let peak = 0;
  let nearZero = 0;
  let clipped = 0;
  for (let i = start; i < samples.length; i++) {
    const value = samples[i];
    energy += value * value;
    mean += value;
    peak = Math.max(peak, Math.abs(value));
    if (Math.abs(value) < 1e-5) nearZero++;
    if (Math.abs(value) >= 0.999) clipped++;
  }
  const count = samples.length - start;
  return { rms: Math.sqrt(energy / count), peak, dc: mean / count, nearZeroPercent: nearZero * 100 / count, clippedSamples: clipped };
}

const dryImpulse = await render({ wet: 0, impulse: true });
const wetImpulse = await render({ wet: 1, impulse: true });
const dry = await render({ wet: 0 });
const mixed = await render({ wet: 0.5 });
const response = {};
for (const frequency of frequencies) {
  const reference = toneLevel(dry, frequency);
  const candidate = toneLevel(mixed, frequency);
  response[frequency] = {
    dry: reference,
    mixed: candidate,
    mixedDbVsDry: 20 * Math.log10(Math.max(1e-12, candidate / reference)),
  };
}

const dryArrival = firstArrival(dryImpulse);
const wetArrival = firstArrival(wetImpulse);
const results = {
  dryArrival,
  wetArrival,
  wetLatencySamples: wetArrival - dryArrival,
  wetLatencyMs: ((wetArrival - dryArrival) / sampleRate) * 1000,
  partialWetResponse: response,
  presets: {},
};

for (const name of ["freshCassette", "wornCassette", "vhsHiFi", "vhsLinear", "dictaphone", "rewindMelt", "chewedTapeExaggerated", "ghostVhsExaggerated"]) {
  const settings = { ...PRESETS[name], sfxEnable: false, sfxLevel: 0 };
  results.presets[name] = {
    signal: signalMetrics(await render({ wet: 1, settings })),
    silence: signalMetrics(await render({ wet: 1, silence: true, settings })),
  };
}

document.querySelector("#results").textContent = JSON.stringify(results, null, 2);
document.body.dataset.complete = "true";
