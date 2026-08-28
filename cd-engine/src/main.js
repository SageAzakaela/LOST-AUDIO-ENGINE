import { buildCdGraph, defaultSettings } from "./audio/graph.js?v=20260827.6";
import { encodeWavMono16 } from "./audio/wav.js";
import { PRESETS } from "./presets.js?v=20260827.3";

const els = {
  fileInput: document.querySelector("#fileInput"),
  playBtn: document.querySelector("#playBtn"),
  stopBtn: document.querySelector("#stopBtn"),
  exportBtn: document.querySelector("#exportBtn"),
  loopToggle: document.querySelector("#loopToggle"),
  triggerDamageBtn: document.querySelector("#triggerDamageBtn"),
  triggerSkipBtn: document.querySelector("#triggerSkipBtn"),

  clarity: document.querySelector("#clarity"),
  damage: document.querySelector("#damage"),
  tracking: document.querySelector("#tracking"),
  jitter: document.querySelector("#jitter"),
  carComp: document.querySelector("#carComp"),
  preset: document.querySelector("#preset"),
  softClip: document.querySelector("#softClip"),

  mode: document.querySelector("#mode"),
  damageShape: document.querySelector("#damageShape"),
  errorRate: document.querySelector("#errorRate"),
  burstMs: document.querySelector("#burstMs"),
  repeatMs: document.querySelector("#repeatMs"),
  scratchRate: document.querySelector("#scratchRate"),
  scratchAmt: document.querySelector("#scratchAmt"),
  correction: document.querySelector("#correction"),
  interpolationMs: document.querySelector("#interpolationMs"),
  rotationHz: document.querySelector("#rotationHz"),
  trackingRate: document.querySelector("#trackingRate"),
  trackingMs: document.querySelector("#trackingMs"),
  servoHunt: document.querySelector("#servoHunt"),
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
  damageShapeVal: document.querySelector("#damageShapeVal"),
  errorRateVal: document.querySelector("#errorRateVal"),
  burstMsVal: document.querySelector("#burstMsVal"),
  repeatMsVal: document.querySelector("#repeatMsVal"),
  scratchRateVal: document.querySelector("#scratchRateVal"),
  scratchAmtVal: document.querySelector("#scratchAmtVal"),
  correctionVal: document.querySelector("#correctionVal"),
  interpolationMsVal: document.querySelector("#interpolationMsVal"),
  rotationHzVal: document.querySelector("#rotationHzVal"),
  trackingRateVal: document.querySelector("#trackingRateVal"),
  trackingMsVal: document.querySelector("#trackingMsVal"),
  servoHuntVal: document.querySelector("#servoHuntVal"),
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

  const errorRate = clamp01(0.006 + c * 0.18 + d * 0.62);
  const burstMs = Math.round(4 + c * 38 + d * 112 + t * 68);
  const repeatMs = Math.round(18 + t * 92 + d * 28);
  const scratchRate = clamp01(0.008 + d * 0.96);
  const scratchAmt = clamp01(0.04 + d * 0.9);
  const correction = clamp01(0.995 - c * 0.42 - d * 0.66 - t * 0.16);
  const interpolationMs = Math.round((3 + c * 9) * 10) / 10;
  const rotationHz = Math.round((5.2 + j * 0.8) * 10) / 10;
  const trackingRate = clamp01(0.004 + t * 0.92);
  const trackingMs = Math.round(45 + t * 820);
  const servoHunt = clamp01(0.03 + t * 0.78 + d * 0.24);
  const jitterMs = Math.round((0.001 + j * 0.32) * 1000) / 1000;
  const jitterRate = Math.round(24 + j * 90);
  const hfLoss = clamp01(0.002 + c * 0.045 + d * 0.12);
  const servoNoise = clamp01(0.015 + t * 0.18 + d * 0.12);
  const mode = t > 0.62 ? "repeat" : correction < 0.36 ? "hold" : "interp";
  const ceiling = Math.round((0.97 - d * 0.11) * 1000) / 1000;
  const outGain = Math.round((0.98 - d * 0.06) * 100) / 100;

  return { mode, errorRate, burstMs, repeatMs, scratchRate, scratchAmt, correction, interpolationMs, rotationHz, trackingRate, trackingMs, servoHunt, jitterMs, jitterRate, hfLoss, servoNoise, ceiling, outGain };
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
    damageShape: els.damageShape.value,
    errorRate: parseFloat(els.errorRate.value),
    burstMs: parseFloat(els.burstMs.value),
    repeatMs: parseFloat(els.repeatMs.value),
    scratchRate: parseFloat(els.scratchRate.value),
    scratchAmt: parseFloat(els.scratchAmt.value),
    correction: parseFloat(els.correction.value),
    interpolationMs: parseFloat(els.interpolationMs.value),
    rotationHz: parseFloat(els.rotationHz.value),
    trackingRate: parseFloat(els.trackingRate.value),
    trackingMs: parseFloat(els.trackingMs.value),
    servoHunt: parseFloat(els.servoHunt.value),
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
  els.softClip.checked = Boolean(s.softClip ?? false);

  els.mode.value = s.mode ?? "interp";
  els.damageShape.value = s.damageShape ?? "radial";
  els.errorRate.value = s.errorRate ?? 0.12;
  els.burstMs.value = s.burstMs ?? 18;
  els.repeatMs.value = s.repeatMs ?? 36;
  els.scratchRate.value = s.scratchRate ?? 0.14;
  els.scratchAmt.value = s.scratchAmt ?? 0.2;
  els.correction.value = s.correction ?? 0.88;
  els.interpolationMs.value = s.interpolationMs ?? 5;
  els.rotationHz.value = s.rotationHz ?? 5.2;
  els.trackingRate.value = s.trackingRate ?? 0.08;
  els.trackingMs.value = s.trackingMs ?? 140;
  els.servoHunt.value = s.servoHunt ?? 0.18;
  els.jitterMs.value = s.jitterMs ?? 0.025;
  els.jitterRate.value = s.jitterRate ?? 34;
  els.hfLoss.value = s.hfLoss ?? 0.025;
  els.servoNoise.value = s.servoNoise ?? 0.08;
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
  els.damageShapeVal.textContent = s.damageShape;
  els.errorRateVal.textContent = pct01(s.errorRate);
  els.burstMsVal.textContent = `${Math.round(s.burstMs)} ms`;
  els.repeatMsVal.textContent = `${Math.round(s.repeatMs)} ms`;
  els.scratchRateVal.textContent = pct01(s.scratchRate);
  els.scratchAmtVal.textContent = pct01(s.scratchAmt);
  els.correctionVal.textContent = pct01(s.correction);
  els.interpolationMsVal.textContent = `${Number(s.interpolationMs).toFixed(2)} ms`;
  els.rotationHzVal.textContent = `${Number(s.rotationHz).toFixed(1)} Hz`;
  els.trackingRateVal.textContent = pct01(s.trackingRate);
  els.trackingMsVal.textContent = `${Math.round(s.trackingMs)} ms`;
  els.servoHuntVal.textContent = pct01(s.servoHunt);
  els.jitterMsVal.textContent = `${Number(s.jitterMs).toFixed(3)} ms`;
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
  els.errorRate.value = t.errorRate;
  els.burstMs.value = t.burstMs;
  els.repeatMs.value = t.repeatMs;
  els.scratchRate.value = t.scratchRate;
  els.scratchAmt.value = t.scratchAmt;
  els.correction.value = t.correction;
  els.interpolationMs.value = t.interpolationMs;
  els.rotationHz.value = t.rotationHz;
  els.trackingRate.value = t.trackingRate;
  els.trackingMs.value = t.trackingMs;
  els.servoHunt.value = t.servoHunt;
  els.jitterMs.value = t.jitterMs;
  els.jitterRate.value = t.jitterRate;
  els.hfLoss.value = t.hfLoss;
  els.servoNoise.value = t.servoNoise;
  els.ceiling.value = t.ceiling;
  els.outGain.value = t.outGain;
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
    els.damageShape,
    els.errorRate,
    els.burstMs,
    els.repeatMs,
    els.scratchRate,
    els.scratchAmt,
    els.correction,
    els.interpolationMs,
    els.rotationHz,
    els.trackingRate,
    els.trackingMs,
    els.servoHunt,
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
      else if (k === "damageShape") els.damageShape.value = preset[k];
      else if (k === "errorRate") els.errorRate.value = preset[k];
      else if (k === "burstMs") els.burstMs.value = preset[k];
      else if (k === "repeatMs") els.repeatMs.value = preset[k];
      else if (k === "scratchRate") els.scratchRate.value = preset[k];
      else if (k === "scratchAmt") els.scratchAmt.value = preset[k];
      else if (k === "correction") els.correction.value = preset[k];
      else if (k === "interpolationMs") els.interpolationMs.value = preset[k];
      else if (k === "rotationHz") els.rotationHz.value = preset[k];
      else if (k === "trackingRate") els.trackingRate.value = preset[k];
      else if (k === "trackingMs") els.trackingMs.value = preset[k];
      else if (k === "servoHunt") els.servoHunt.value = preset[k];
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
  els.triggerDamageBtn.addEventListener("click", () => {
    if (!realtime.src || !realtime.graph) {
      setState("Play audio before triggering damage");
      return;
    }
    realtime.graph.triggerDamage(1);
    setState("Damage triggered");
  });
  els.triggerSkipBtn.addEventListener("click", () => {
    if (!realtime.src || !realtime.graph) {
      setState("Play audio before triggering a skip");
      return;
    }
    realtime.graph.triggerSkip(1);
    setState("Skip triggered");
  });
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
