import { buildTapeGraph, defaultSettings as tapeDefaults } from "../tape-engine/src/audio/graph.js";
import { buildTelevisionGraph, defaultSettings as tvDefaults } from "../television-engine/src/audio/graph.js";
import { buildCamcorderGraph, defaultSettings as camDefaults } from "../camcorder-engine/src/audio/graph.js?v=20260828.28";

const sampleRate = 48000;
const seconds = 2;

async function decode(url) {
  const bytes = await (await fetch(url)).arrayBuffer();
  const ctx = new AudioContext({ sampleRate });
  try { return await ctx.decodeAudioData(bytes.slice(0)); }
  finally { await ctx.close(); }
}

function stats(buffer) {
  const data = buffer.getChannelData(0); let peak = 0; let sum = 0; let nonFinite = 0;
  for (const value of data) { if (!Number.isFinite(value)) { nonFinite++; continue; } peak = Math.max(peak, Math.abs(value)); sum += value * value; }
  return { peak, rms: Math.sqrt(sum / data.length), nonFinite };
}

async function render(kind, audioBuffer) {
  const ctx = new OfflineAudioContext(1, sampleRate * seconds, sampleRate);
  let graph; let settings; let target;
  if (kind === "tape") {
    graph = await buildTapeGraph(ctx, { seed: 1 }); settings = { ...tapeDefaults(), hiss: 0, hum: 0, dropout: 0, glitch: 0, sfxEnable: true, sfxLevel: 1 }; target = graph.sfx;
  } else if (kind === "television") {
    graph = await buildTelevisionGraph(ctx, { seed: 1 }); settings = { ...tvDefaults(), static: 0, hum: 0, whine: 0, noiseCrackle: 0, bedEnable: true, bedLevel: 1 }; target = graph.nodes.sfx;
  } else {
    graph = await buildCamcorderGraph(ctx, { seed: 1 }); settings = { ...camDefaults(), wind: false, handling: 0, rub: 0, hiss: 0, motorBleed: 0, drop: 0, chirp: 0 }; target = graph.camera;
  }
  graph.applySettings(settings, { time: 0, ramp: 0 }); graph.output.connect(ctx.destination);
  const source = new AudioBufferSourceNode(ctx, { buffer: audioBuffer, loop: true }); source.connect(target); source.start(0); source.stop(seconds);
  return stats(await ctx.startRendering());
}

try {
  const [tape, television, camcorder] = await Promise.all([
    decode("../tape-engine/audio/OASD_Casette_Working_09.wav"),
    decode("../television-engine/audio/crt.mp3"),
    decode("../camcorder-engine/audio/a_early_2000s_camcor_%231-1770665742065.mp3"),
  ]);
  const results = {
    tape: await render("tape", tape),
    television: await render("television", television),
    camcorderWindOff: await render("camcorder", camcorder),
  };
  for (const [name, value] of Object.entries(results)) {
    value.active = value.rms > 1e-4 && value.peak > 1e-3 && value.nonFinite === 0;
    if (!value.active) throw new Error(`${name} bed is not audible`);
  }
  document.body.dataset.complete = "true"; document.body.dataset.status = "pass"; document.querySelector("#out").textContent = JSON.stringify(results, null, 2);
} catch (error) {
  document.body.dataset.complete = "true"; document.body.dataset.status = "fail"; document.querySelector("#out").textContent = String(error?.stack || error);
}
