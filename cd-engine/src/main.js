import { buildCdGraph, defaultSettings } from "./audio/graph.js";
import { encodeWavMono16 } from "./audio/wav.js";
import { PRESETS } from "./presets.js";

const els = {
  fileInput: document.querySelector("#fileInput"),
  playBtn: document.querySelector("#playBtn"),
  stopBtn: document.querySelector("#stopBtn"),
  exportBtn: document.querySelector("#exportBtn"),
  loopToggle: document.querySelector("#loopToggle"),

  clarity: document.querySelector("#clarity"),
  damage: document.querySelector("#damage"),
  tracking: document.querySelector("#tracking"),
  jitter: document.querySelector("#jitter"),
  carComp: document.querySelector("#carComp"),
  preset: document.querySelector("#preset"),
  softClip: document.querySelector("#softClip"),

  mode: document.querySelector("#mode"),
  errorRate: document.querySelector("#errorRate"),
  burstMs: document.querySelector("#burstMs"),
  repeatMs: document.querySelector("#repeatMs"),
  scratchRate: document.querySelector("#scratchRate"),
  scratchAmt: document.querySelector("#scratchAmt"),
  jitterMs: document.querySelector("#jitterMs"),
  jitterRate: document.querySelector("#jitterRate"),
  hfLoss: document.querySelector("#hfLoss"),
  servoNoise: document.querySelector("#servoNoise"),
  ceiling: document.querySelector("#ceiling"),
  outGain: document.querySelector("#outGain"),

  clarityVal: document.querySelector("#clarityVal"),
  damageVal: document.querySelector("#damageVal"),
  trackingVal: document.querySelector("#trackingVal"),
  jitterVal: document.querySelector("#jitterVal"),
  carCompVal: document.querySelector("#carCompVal"),
  modeVal: document.querySelector("#modeVal"),
  errorRateVal: document.querySelector("#errorRateVal"),
  burstMsVal: document.querySelector("#burstMsVal"),
  repeatMsVal: document.querySelector("#repeatMsVal"),
  scratchRateVal: document.querySelector("#scratchRateVal"),
  scratchAmtVal: document.querySelector("#scratchAmtVal"),
  jitterMsVal: document.querySelector("#jitterMsVal"),
  jitterRateVal: document.querySelector("#jitterRateVal"),
  hfLossVal: document.querySelector("#hfLossVal"),
  servoNoiseVal: document.querySelector("#servoNoiseVal"),
  ceilingVal: document.querySelector("#ceilingVal"),
  outGainVal: document.querySelector("#outGainVal"),

  fileName: document.querySelector("#fileName"),
  duration: document.querySelector("#duration"),
  sampleRate: document.querySelector("#sampleRate"),
  state: document.querySelector("#state"),
};

function clamp01(x) {
  return Math.min(1, Math.max(0, x));
}
function pct01(x) {
  return `${Math.round(clamp01(x) * 100)}%`;
}
function fmtTime(seconds) {
  if (!Number.isFinite(seconds)) return "-";
  const m = Math.floor(seconds / 60);
  const s = Math.floor(seconds % 60);
  return `${m}:${String(s).padStart(2, "0")}`;
}

function fnv1a32Sampled(bytes) {
  let h = 0x811c9dc5 >>> 0;
  const step = Math.max(1, Math.floor(bytes.length / 16384));
  for (let i = 0; i < bytes.length; i += step) {
    h ^= bytes[i];
    h = Math.imul(h, 0x01000193) >>> 0;
  }
  h ^= bytes.length >>> 0;
  h = Math.imul(h, 0x01000193) >>> 0;
  return h >>> 0;
}

const realtime = { ctx: null, graph: null, src: null, seed: 0 };
let audioBuffer = null;
let audioDataSeed = 0;

let settings = defaultSettings();

function setState(text) {
  els.state.textContent = text;
}

function computeMacroTargets(primary) {
  const clarity = clamp01(primary.clarity ?? 0.65);
  const damage = clamp01(primary.damage ?? 0.25);
  const tracking = clamp01(primary.tracking ?? 0.22);
  const jitter = clamp01(primary.jitter ?? 0.18);

  const c = Math.pow(1 - clarity, 1.35);
  const d = Math.pow(damage, 1.25);
  const t = Math.pow(tracking, 1.35);
  const j = Math.pow(jitter, 1.25);

  const errorRate = clamp01(0.02 + c * 0.6 + t * 0.25);
  const burstMs = Math.round(8 + (c * 60 + t * 120) * (0.35 + 0.65 * d));
  const repeatMs = Math.round(18 + t * 120);

  const scratchRate = clamp01(0.03 + d * 0.85);
  const scratchAmt = clamp01(0.08 + d * 0.75);

  const jitterMs = Math.round((0.02 + j * 0.75) * 100) / 100;
  const jitterRate = Math.round(18 + j * 85);

  const hfLoss = clamp01(0.02 + c * 0.12 + d * 0.22);
  const servoNoise = clamp01(0.03 + t * 0.25 + d * 0.1);

  const mode = errorRate > 0.52 ? "repeat" : errorRate > 0.25 ? "hold" : "interp";
  const ceiling = 0.96 - d * 0.08;
  const outGain = Math.round((0.98 + d * 0.12) * 100) / 100;

  const carComp = clamp01(t * 0.65 + d * 0.2);

  return { mode, errorRate, burstMs, repeatMs, scratchRate, scratchAmt, jitterMs, jitterRate, hfLoss, servoNoise, ceiling, outGain, carComp };
}

function readSettingsFromUI() {
  return {
    clarity: parseFloat(els.clarity.value),
    damage: parseFloat(els.damage.value),
    tracking: parseFloat(els.tracking.value),
    jitter: parseFloat(els.jitter.value),
    carComp: parseFloat(els.carComp.value),
    softClip: Boolean(els.softClip.checked),

    mode: els.mode.value,
    errorRate: parseFloat(els.errorRate.value),
    burstMs: parseFloat(els.burstMs.value),
    repeatMs: parseFloat(els.repeatMs.value),
    scratchRate: parseFloat(els.scratchRate.value),
    scratchAmt: parseFloat(els.scratchAmt.value),
    jitterMs: parseFloat(els.jitterMs.value),
    jitterRate: parseFloat(els.jitterRate.value),
    hfLoss: parseFloat(els.hfLoss.value),
    servoNoise: parseFloat(els.servoNoise.value),
    ceiling: parseFloat(els.ceiling.value),
    outGain: parseFloat(els.outGain.value),
  };
}

function writeSettingsToUI(s) {
  els.clarity.value = s.clarity ?? 0.65;
  els.damage.value = s.damage ?? 0.25;
  els.tracking.value = s.tracking ?? 0.22;
  els.jitter.value = s.jitter ?? 0.18;
  els.carComp.value = s.carComp ?? 0;
  els.softClip.checked = Boolean(s.softClip ?? true);

  els.mode.value = s.mode ?? "hold";
  els.errorRate.value = s.errorRate ?? 0.18;
  els.burstMs.value = s.burstMs ?? 24;
  els.repeatMs.value = s.repeatMs ?? 42;
  els.scratchRate.value = s.scratchRate ?? 0.25;
  els.scratchAmt.value = s.scratchAmt ?? 0.35;
  els.jitterMs.value = s.jitterMs ?? 0.18;
  els.jitterRate.value = s.jitterRate ?? 38;
  els.hfLoss.value = s.hfLoss ?? 0.1;
  els.servoNoise.value = s.servoNoise ?? 0.12;
  els.ceiling.value = s.ceiling ?? 0.94;
  els.outGain.value = s.outGain ?? 0.98;
  refreshValueLabels();
}

function refreshValueLabels() {
  const s = readSettingsFromUI();
  els.clarityVal.textContent = pct01(s.clarity);
  els.damageVal.textContent = pct01(s.damage);
  els.trackingVal.textContent = pct01(s.tracking);
  els.jitterVal.textContent = pct01(s.jitter);
  els.carCompVal.textContent = pct01(s.carComp);
  els.modeVal.textContent = s.mode;
  els.errorRateVal.textContent = pct01(s.errorRate);
  els.burstMsVal.textContent = `${Math.round(s.burstMs)} ms`;
  els.repeatMsVal.textContent = `${Math.round(s.repeatMs)} ms`;
  els.scratchRateVal.textContent = pct01(s.scratchRate);
  els.scratchAmtVal.textContent = pct01(s.scratchAmt);
  els.jitterMsVal.textContent = `${Number(s.jitterMs).toFixed(2)} ms`;
  els.jitterRateVal.textContent = `${Math.round(s.jitterRate)} Hz`;
  els.hfLossVal.textContent = pct01(s.hfLoss);
  els.servoNoiseVal.textContent = pct01(s.servoNoise);
  els.ceilingVal.textContent = `${Math.round(s.ceiling * 100)}%`;
  els.outGainVal.textContent = `${s.outGain.toFixed(2)}x`;
}

function applyMacrosFromPrimaryToAdvanced() {
  const primary = {
    clarity: parseFloat(els.clarity.value),
    damage: parseFloat(els.damage.value),
    tracking: parseFloat(els.tracking.value),
    jitter: parseFloat(els.jitter.value),
  };
  const t = computeMacroTargets(primary);
  els.mode.value = t.mode;
  els.errorRate.value = t.errorRate;
  els.burstMs.value = t.burstMs;
  els.repeatMs.value = t.repeatMs;
  els.scratchRate.value = t.scratchRate;
  els.scratchAmt.value = t.scratchAmt;
  els.jitterMs.value = t.jitterMs;
  els.jitterRate.value = t.jitterRate;
  els.hfLoss.value = t.hfLoss;
  els.servoNoise.value = t.servoNoise;
  els.ceiling.value = t.ceiling;
  els.outGain.value = t.outGain;
  els.carComp.value = t.carComp;
  refreshValueLabels();
}

async function ensureRealtimeGraph() {
  if (realtime.ctx && realtime.graph) return;
  if (!audioBuffer) throw new Error("No audio loaded");
  realtime.ctx = new (window.AudioContext || window.webkitAudioContext)({
    numberOfChannels: 1,
    sampleRate: audioBuffer.sampleRate,
  });
  realtime.graph = await buildCdGraph(realtime.ctx, { seed: audioDataSeed });
  realtime.graph.output.connect(realtime.ctx.destination);
  realtime.seed = audioDataSeed >>> 0;
  realtime.graph.applySettings(readSettingsFromUI(), { ramp: 0 });
}

async function startPlayback() {
  if (!audioBuffer) return;
  await ensureRealtimeGraph();
  if (!realtime.ctx || !realtime.graph) return;
  stopPlayback();
  await realtime.ctx.resume();

  const s = readSettingsFromUI();
  realtime.graph.applySettings(s, { ramp: 0.03 });
  realtime.graph.reset(realtime.seed);

  const src = new AudioBufferSourceNode(realtime.ctx, { buffer: audioBuffer });
  src.loop = Boolean(els.loopToggle.checked);
  src.loopStart = 0;
  src.loopEnd = audioBuffer.duration;
  src.connect(realtime.graph.input);
  src.onended = () => {
    if (realtime.src === src) {
      realtime.src = null;
      setState("Stopped");
    }
  };
  realtime.src = src;
  src.start(0);
  setState("Playing");
  els.stopBtn.disabled = false;
}

function stopPlayback() {
  if (realtime.src) {
    try {
      realtime.src.stop();
    } catch {
      // ignore
    }
    try {
      realtime.src.disconnect();
    } catch {
      // ignore
    }
    realtime.src = null;
  }
  setState(audioBuffer ? "Ready" : "Idle");
}

async function exportWav() {
  if (!audioBuffer) return;
  els.exportBtn.disabled = true;
  setState("Rendering...");
  const s = readSettingsFromUI();
  const offline = new OfflineAudioContext({
    numberOfChannels: 1,
    length: audioBuffer.length,
    sampleRate: audioBuffer.sampleRate,
  });
  const graph = await buildCdGraph(offline, { seed: audioDataSeed });
  graph.applySettings(s, { time: 0, ramp: 0 });
  const src = new AudioBufferSourceNode(offline, { buffer: audioBuffer });
  src.connect(graph.input);
  graph.output.connect(offline.destination);
  src.start(0);
  const rendered = await offline.startRendering();
  const blob = encodeWavMono16(rendered);
  const url = URL.createObjectURL(blob);
  const base = (els.fileName.textContent || "export").replace(/\.[^/.]+$/, "");
  const a = document.createElement("a");
  a.href = url;
  a.download = `${base} - CD Engine.wav`;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 5000);
  setState("Exported");
  els.exportBtn.disabled = false;
}

function applyToGraphAndMarkCustom() {
  refreshValueLabels();
  settings = readSettingsFromUI();
  if (realtime.graph && realtime.ctx) realtime.graph.applySettings(settings, { ramp: 0.03 });
  els.preset.value = "";
}

function hookControls() {
  for (const el of [els.clarity, els.damage, els.tracking, els.jitter]) {
    el.addEventListener("input", () => {
      applyMacrosFromPrimaryToAdvanced();
      applyToGraphAndMarkCustom();
    });
  }
  for (const el of [
    els.carComp,
    els.softClip,
    els.mode,
    els.errorRate,
    els.burstMs,
    els.repeatMs,
    els.scratchRate,
    els.scratchAmt,
    els.jitterMs,
    els.jitterRate,
    els.hfLoss,
    els.servoNoise,
    els.ceiling,
    els.outGain,
  ]) {
    const evt = el.tagName === "SELECT" || el.type === "checkbox" ? "change" : "input";
    el.addEventListener(evt, applyToGraphAndMarkCustom);
  }

  els.preset.addEventListener("change", () => {
    const key = els.preset.value;
    if (!key) return;
    const preset = PRESETS[key];
    if (!preset) return;
    writeSettingsToUI({ ...defaultSettings(), ...preset });
    applyMacrosFromPrimaryToAdvanced();
    // Re-apply explicit advanced overrides from preset.
    for (const k of Object.keys(preset)) {
      if (k === "softClip") els.softClip.checked = Boolean(preset[k]);
      else if (k === "carComp") els.carComp.value = preset[k];
      else if (k === "mode") els.mode.value = preset[k];
      else if (k === "errorRate") els.errorRate.value = preset[k];
      else if (k === "burstMs") els.burstMs.value = preset[k];
      else if (k === "repeatMs") els.repeatMs.value = preset[k];
      else if (k === "scratchRate") els.scratchRate.value = preset[k];
      else if (k === "scratchAmt") els.scratchAmt.value = preset[k];
      else if (k === "jitterMs") els.jitterMs.value = preset[k];
      else if (k === "jitterRate") els.jitterRate.value = preset[k];
      else if (k === "hfLoss") els.hfLoss.value = preset[k];
      else if (k === "servoNoise") els.servoNoise.value = preset[k];
      else if (k === "ceiling") els.ceiling.value = preset[k];
      else if (k === "outGain") els.outGain.value = preset[k];
    }
    refreshValueLabels();
    if (realtime.graph && realtime.ctx) realtime.graph.applySettings(readSettingsFromUI(), { ramp: 0.03 });
  });
}

function wireButtons() {
  els.playBtn.addEventListener("click", () => startPlayback());
  els.stopBtn.addEventListener("click", () => stopPlayback());
  els.exportBtn.addEventListener("click", () => exportWav());
  els.loopToggle.addEventListener("change", () => {
    if (!realtime.src || !audioBuffer) return;
    realtime.src.loop = Boolean(els.loopToggle.checked);
    realtime.src.loopStart = 0;
    realtime.src.loopEnd = audioBuffer.duration;
  });
}

async function decodeFile(file) {
  const arrayBuffer = await file.arrayBuffer();
  audioDataSeed = fnv1a32Sampled(new Uint8Array(arrayBuffer));
  const tmpCtx = new (window.AudioContext || window.webkitAudioContext)();
  try {
    const decoded = await tmpCtx.decodeAudioData(arrayBuffer.slice(0));
    const mono = new AudioBuffer({ length: decoded.length, sampleRate: decoded.sampleRate, numberOfChannels: 1 });
    const out = mono.getChannelData(0);
    if (decoded.numberOfChannels === 1) out.set(decoded.getChannelData(0));
    else {
      const a = decoded.getChannelData(0);
      const b = decoded.getChannelData(1);
      for (let i = 0; i < out.length; i++) out[i] = 0.5 * (a[i] + b[i]);
    }
    audioBuffer = mono;
  } finally {
    await tmpCtx.close();
  }
}

els.fileInput.addEventListener("change", async () => {
  const file = els.fileInput.files?.[0];
  if (!file) return;
  setState("Decoding...");
  els.playBtn.disabled = true;
  els.exportBtn.disabled = true;
  try {
    stopPlayback();
    await decodeFile(file);
    if (realtime.ctx && realtime.ctx.sampleRate !== audioBuffer.sampleRate) {
      await realtime.ctx.close();
      realtime.ctx = null;
      realtime.graph = null;
      realtime.src = null;
    }
    els.fileName.textContent = file.name;
    els.duration.textContent = fmtTime(audioBuffer.duration);
    els.sampleRate.textContent = `${audioBuffer.sampleRate} Hz`;
    setState("Ready");
    els.playBtn.disabled = false;
    els.exportBtn.disabled = false;
  } catch (e) {
    console.error(e);
    audioBuffer = null;
    setState("Decode failed");
    els.fileName.textContent = "None";
    els.duration.textContent = "-";
    els.sampleRate.textContent = "-";
  }
});

hookControls();
wireButtons();
writeSettingsToUI(settings);
applyMacrosFromPrimaryToAdvanced();
setState("Idle");
