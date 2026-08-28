import { buildConferenceGraph, defaultSettings } from "../conference-engine/src/audio/graph.js?v=20260827.24";
import { PRESETS } from "../conference-engine/src/presets.js?v=20260827.24";

const sampleRate = 48000;
const seconds = 3;
const frames = sampleRate * seconds;
const seed = 0x434f4e46;
const clamp01 = (x) => Math.min(1, Math.max(0, x));

function macroTargets(p) {
  const mode = p.mode ?? "discord";
  const bandwidth = clamp01(p.bandwidth ?? 0.45);
  const codec = clamp01(p.codec ?? 0.35);
  const dropouts = clamp01(p.dropouts ?? 0.25);
  const jitter = clamp01(p.jitter ?? 0.2);
  const noise = clamp01(p.noise ?? 0.12);
  const narrow = Math.pow(1 - bandwidth, 1.35);
  const c = Math.pow(codec, 1.25);
  const d = Math.pow(dropouts, 1.5);
  const j = Math.pow(jitter, 1.2);
  const n = Math.pow(noise, 1.15);
  const base = mode === "cell"
    ? { hp: 230, hpR: 310, lp: 3600, lpR: 1500, mid: 1900, midR: 520, hump: 2.2, out: 1.02, ceil: 0.92, frame: 20, burst: 0.68, suppress: 0.5, agc: 0.46 }
    : mode === "skype"
      ? { hp: 170, hpR: 300, lp: 4800, lpR: 1800, mid: 1700, midR: 440, hump: 2, out: 0.98, ceil: 0.92, frame: 30, burst: 0.58, suppress: 0.34, agc: 0.32 }
      : mode === "zoom"
        ? { hp: 120, hpR: 260, lp: 7600, lpR: 2500, mid: 2100, midR: 600, hump: 1.4, out: 0.98, ceil: 0.94, frame: 10, burst: 0.48, suppress: 0.62, agc: 0.58 }
        : { hp: 100, hpR: 280, lp: 7200, lpR: 2300, mid: 2000, midR: 560, hump: 1.6, out: 0.98, ceil: 0.93, frame: 20, burst: 0.56, suppress: 0.43, agc: 0.36 };
  return {
    hpHz: Math.round(base.hp + narrow * base.hpR),
    lpHz: Math.round(base.lp - narrow * base.lpR),
    midFreq: Math.round(base.mid + (0.45 - narrow) * base.midR),
    midHumpDb: Math.round((base.hump + narrow * 2.8) * 20) / 20,
    concealMode: d > 0.68 ? "repeat" : d > 0.28 ? "hold" : "interp",
    packetLoss: clamp01(0.002 + d * (mode === "cell" ? 0.36 : 0.28)),
    packetMs: Math.round(base.frame + d * 12),
    repeatMs: Math.round(22 + d * 82),
    jitterMs: Math.round((0.04 + j * 6.4) * 100) / 100,
    jitterRate: Math.round(5 + j * 36),
    bits: Math.round(15 - c * 7),
    rate: Math.round(48000 - c * (mode === "cell" ? 39000 : mode === "skype" ? 36500 : 33500)),
    gate: clamp01(0.035 + base.suppress * 0.08 + c * 0.13 + d * 0.1),
    burstiness: clamp01(base.burst + d * 0.28),
    suppression: clamp01(base.suppress + c * 0.22 + d * 0.08),
    agc: clamp01(base.agc + c * 0.12),
    bufferSlip: clamp01(0.008 + j * 0.42 + d * 0.08),
    bandwidthSwitch: clamp01(0.015 + c * 0.18 + j * 0.2),
    comfortNoise: clamp01(0.06 + n * 0.62),
    ceiling: clamp01(base.ceil - c * 0.05),
    outGain: Math.round((base.out + c * 0.12) * 100) / 100,
  };
}

function makeSpeech(context) {
  const buffer = context.createBuffer(1, frames, sampleRate);
  const data = buffer.getChannelData(0);
  let state = 0x18d63a91;
  for (let i = 0; i < frames; i++) {
    const t = i / sampleRate;
    state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
    const breath = (state / 0xffffffff) * 2 - 1;
    const phrase = i % Math.round(sampleRate * 0.92);
    const on = phrase < sampleRate * 0.63;
    const syllable = on ? (0.5 + 0.5 * Math.sin(2 * Math.PI * 4.2 * t)) : 0.012;
    data[i] = syllable * (
      0.24 * Math.sin(2 * Math.PI * 118 * t) +
      0.15 * Math.sin(2 * Math.PI * 236 * t) +
      0.11 * Math.sin(2 * Math.PI * 826 * t) +
      0.06 * Math.sin(2 * Math.PI * 1920 * t) +
      breath * 0.025
    );
  }
  return buffer;
}

async function render(settings) {
  const context = new OfflineAudioContext(1, frames, sampleRate);
  const graph = await buildConferenceGraph(context, { seed });
  graph.applySettings(settings, { time: 0, ramp: 0 });
  graph.reset(seed);
  graph.output.connect(context.destination);
  const source = new AudioBufferSourceNode(context, { buffer: makeSpeech(context) });
  source.connect(graph.input);
  source.start();
  return context.startRendering();
}

function metrics(buffer, ceiling) {
  const data = buffer.getChannelData(0);
  let energy = 0, peak = 0, nonFinite = 0, clipped = 0, discontinuities = 0;
  for (let i = 0; i < data.length; i++) {
    const sample = data[i];
    if (!Number.isFinite(sample)) { nonFinite++; continue; }
    const a = Math.abs(sample);
    energy += sample * sample;
    peak = Math.max(peak, a);
    if (a >= ceiling - 0.00005) clipped++;
    if (i && Math.abs(sample - data[i - 1]) > 0.35) discontinuities++;
  }
  return {
    peak,
    rms: Math.sqrt(energy / data.length),
    nonFinite,
    ceilingSafe: peak <= ceiling + 0.0001,
    clippedPercent: clipped * 100 / data.length,
    discontinuities,
  };
}

const results = {};
for (const [key, preset] of Object.entries(PRESETS)) {
  const settings = { ...defaultSettings(), ...macroTargets(preset), ...preset };
  results[key] = metrics(await render(settings), settings.ceiling);
  document.querySelector("#results").textContent = JSON.stringify(results, null, 2);
}
document.body.dataset.complete = "true";
