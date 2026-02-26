import { buildConferenceGraph, defaultSettings } from "./audio/graph.js";
import { encodeWavMono16 } from "./audio/wav.js";
import { PRESETS } from "./presets.js";

const els = {
  fileInput: document.querySelector("#fileInput"),
  playBtn: document.querySelector("#playBtn"),
  stopBtn: document.querySelector("#stopBtn"),
  exportBtn: document.querySelector("#exportBtn"),
  loopToggle: document.querySelector("#loopToggle"),

  mode: document.querySelector("#mode"),
  bandwidth: document.querySelector("#bandwidth"),
  codec: document.querySelector("#codec"),
  dropouts: document.querySelector("#dropouts"),
  jitter: document.querySelector("#jitter"),
  robot: document.querySelector("#robot"),
  noise: document.querySelector("#noise"),
  preset: document.querySelector("#preset"),

  hpHz: document.querySelector("#hpHz"),
  lpHz: document.querySelector("#lpHz"),
  midHumpDb: document.querySelector("#midHumpDb"),
  midFreq: document.querySelector("#midFreq"),
  concealMode: document.querySelector("#concealMode"),
  packetLoss: document.querySelector("#packetLoss"),
  packetMs: document.querySelector("#packetMs"),
  repeatMs: document.querySelector("#repeatMs"),
  jitterMs: document.querySelector("#jitterMs"),
  jitterRate: document.querySelector("#jitterRate"),
  gate: document.querySelector("#gate"),
  bits: document.querySelector("#bits"),
  rate: document.querySelector("#rate"),
  ceiling: document.querySelector("#ceiling"),
  outGain: document.querySelector("#outGain"),

  modeVal: document.querySelector("#modeVal"),
  bandwidthVal: document.querySelector("#bandwidthVal"),
  codecVal: document.querySelector("#codecVal"),
  dropoutsVal: document.querySelector("#dropoutsVal"),
  jitterVal: document.querySelector("#jitterVal"),
  robotVal: document.querySelector("#robotVal"),
  noiseVal: document.querySelector("#noiseVal"),

  hpHzVal: document.querySelector("#hpHzVal"),
  lpHzVal: document.querySelector("#lpHzVal"),
  midHumpDbVal: document.querySelector("#midHumpDbVal"),
  midFreqVal: document.querySelector("#midFreqVal"),
  concealModeVal: document.querySelector("#concealModeVal"),
  packetLossVal: document.querySelector("#packetLossVal"),
  packetMsVal: document.querySelector("#packetMsVal"),
  repeatMsVal: document.querySelector("#repeatMsVal"),
  jitterMsVal: document.querySelector("#jitterMsVal"),
  jitterRateVal: document.querySelector("#jitterRateVal"),
  gateVal: document.querySelector("#gateVal"),
  bitsVal: document.querySelector("#bitsVal"),
  rateVal: document.querySelector("#rateVal"),
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
function fmtHz(hz) {
  if (!Number.isFinite(hz)) return "-";
  if (hz >= 1000) return `${(hz / 1000).toFixed(2)} kHz`;
  return `${Math.round(hz)} Hz`;
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

function readPrimaryFromUI() {
  return {
    mode: els.mode.value,
    bandwidth: parseFloat(els.bandwidth.value),
    codec: parseFloat(els.codec.value),
    dropouts: parseFloat(els.dropouts.value),
    jitter: parseFloat(els.jitter.value),
    robot: parseFloat(els.robot.value),
    noise: parseFloat(els.noise.value),
  };
}

function computeMacroTargets(primary) {
  const mode = primary.mode ?? "discord";
  const bandwidth = clamp01(primary.bandwidth ?? 0.45);
  const codec = clamp01(primary.codec ?? 0.35);
  const dropouts = clamp01(primary.dropouts ?? 0.25);
  const jitter = clamp01(primary.jitter ?? 0.2);
  const robot = clamp01(primary.robot ?? 0.12);
  const noise = clamp01(primary.noise ?? 0.12);

  const narrow = Math.pow(1 - bandwidth, 1.35);
  const c = Math.pow(codec, 1.25);
  const d = Math.pow(dropouts, 1.3);
  const j = Math.pow(jitter, 1.2);
  const r = Math.pow(robot, 1.25);
  const n = Math.pow(noise, 1.2);

  const base =
    mode === "cell"
      ? { hp: 260, hpR: 380, lp: 3600, lpR: 1700, mid: 1900, midR: 520, hump: 2.2, out: 1.02, ceil: 0.92 }
      : mode === "skype"
        ? { hp: 220, hpR: 340, lp: 4200, lpR: 1600, mid: 1700, midR: 440, hump: 2.0, out: 0.98, ceil: 0.92 }
        : mode === "zoom"
          ? { hp: 180, hpR: 260, lp: 6200, lpR: 1900, mid: 2100, midR: 600, hump: 1.5, out: 0.98, ceil: 0.94 }
          : { hp: 210, hpR: 320, lp: 5200, lpR: 1700, mid: 2000, midR: 560, hump: 1.8, out: 0.98, ceil: 0.93 };

  const hpHz = Math.round(base.hp + narrow * base.hpR);
  const lpHz = Math.round(base.lp - narrow * base.lpR);
  const midFreq = Math.round(base.mid + (0.45 - narrow) * base.midR);
  const midHumpDb = Math.round((base.hump + narrow * 2.8) * 20) / 20;

  const concealMode = d > 0.62 ? "repeat" : d > 0.22 ? "hold" : "interp";
  const packetLoss = clamp01(0.02 + d * (mode === "cell" ? 0.75 : 0.55));
  const packetMs = Math.round(12 + d * (mode === "zoom" ? 70 : 95));
  const repeatMs = Math.round(18 + d * 120);

  const jitterMs = Math.round((0.02 + j * 0.55) * 100) / 100;
  const jitterRate = Math.round(18 + j * 80);

  const bits = Math.round(14 - c * 8);
  const rate = Math.round(46000 - c * (mode === "cell" ? 38000 : 32000));

  const gate = clamp01(0.05 + (mode === "zoom" ? 0.12 : 0.08) + c * 0.25 + d * 0.2);

  const ceiling = clamp01(base.ceil - c * 0.05);
  const outGain = Math.round((base.out + c * 0.12) * 100) / 100;

  return {
    hpHz,
    lpHz,
    midHumpDb,
    midFreq,
    concealMode,
    packetLoss,
    packetMs,
    repeatMs,
    jitterMs,
    jitterRate,
    bits,
    rate,
    gate,
    ceiling,
    outGain,
    robot: r,
    noise: n,
  };
}

function readSettingsFromUI() {
  return {
    ...readPrimaryFromUI(),
    hpHz: parseFloat(els.hpHz.value),
    lpHz: parseFloat(els.lpHz.value),
    midHumpDb: parseFloat(els.midHumpDb.value),
    midFreq: parseFloat(els.midFreq.value),
    concealMode: els.concealMode.value,
    packetLoss: parseFloat(els.packetLoss.value),
    packetMs: parseFloat(els.packetMs.value),
    repeatMs: parseFloat(els.repeatMs.value),
    jitterMs: parseFloat(els.jitterMs.value),
    jitterRate: parseFloat(els.jitterRate.value),
    gate: parseFloat(els.gate.value),
    bits: parseFloat(els.bits.value),
    rate: parseFloat(els.rate.value),
    ceiling: parseFloat(els.ceiling.value),
    outGain: parseFloat(els.outGain.value),
  };
}

function writeSettingsToUI(s) {
  els.mode.value = s.mode ?? "discord";
  els.bandwidth.value = s.bandwidth ?? 0.45;
  els.codec.value = s.codec ?? 0.35;
  els.dropouts.value = s.dropouts ?? 0.25;
  els.jitter.value = s.jitter ?? 0.2;
  els.robot.value = s.robot ?? 0.12;
  els.noise.value = s.noise ?? 0.12;

  els.hpHz.value = s.hpHz ?? 260;
  els.lpHz.value = s.lpHz ?? 4200;
  els.midHumpDb.value = s.midHumpDb ?? 2.2;
  els.midFreq.value = s.midFreq ?? 1750;
  els.concealMode.value = s.concealMode ?? "hold";
  els.packetLoss.value = s.packetLoss ?? 0.18;
  els.packetMs.value = s.packetMs ?? 24;
  els.repeatMs.value = s.repeatMs ?? 42;
  els.jitterMs.value = s.jitterMs ?? 0.12;
  els.jitterRate.value = s.jitterRate ?? 34;
  els.gate.value = s.gate ?? 0.12;
  els.bits.value = s.bits ?? 12;
  els.rate.value = s.rate ?? 24000;
  els.ceiling.value = s.ceiling ?? 0.92;
  els.outGain.value = s.outGain ?? 0.98;
  refreshValueLabels();
}

function refreshValueLabels() {
  const s = readSettingsFromUI();
  els.modeVal.textContent = s.mode;
  els.bandwidthVal.textContent = pct01(s.bandwidth);
  els.codecVal.textContent = pct01(s.codec);
  els.dropoutsVal.textContent = pct01(s.dropouts);
  els.jitterVal.textContent = pct01(s.jitter);
  els.robotVal.textContent = pct01(s.robot);
  els.noiseVal.textContent = pct01(s.noise);

  els.hpHzVal.textContent = fmtHz(s.hpHz);
  els.lpHzVal.textContent = fmtHz(s.lpHz);
  els.midHumpDbVal.textContent = `${Number(s.midHumpDb).toFixed(1)} dB`;
  els.midFreqVal.textContent = fmtHz(s.midFreq);
  els.concealModeVal.textContent = s.concealMode;
  els.packetLossVal.textContent = pct01(s.packetLoss);
  els.packetMsVal.textContent = `${Math.round(s.packetMs)} ms`;
  els.repeatMsVal.textContent = `${Math.round(s.repeatMs)} ms`;
  els.jitterMsVal.textContent = `${Number(s.jitterMs).toFixed(2)} ms`;
  els.jitterRateVal.textContent = `${Math.round(s.jitterRate)} Hz`;
  els.gateVal.textContent = pct01(s.gate);
  els.bitsVal.textContent = `${Math.round(s.bits)}-bit`;
  els.rateVal.textContent = fmtHz(s.rate);
  els.ceilingVal.textContent = `${Math.round(s.ceiling * 100)}%`;
  els.outGainVal.textContent = `${s.outGain.toFixed(2)}x`;
}

function applyMacrosFromPrimaryToAdvanced() {
  const t = computeMacroTargets(readPrimaryFromUI());
  els.hpHz.value = t.hpHz;
  els.lpHz.value = t.lpHz;
  els.midHumpDb.value = t.midHumpDb;
  els.midFreq.value = t.midFreq;
  els.concealMode.value = t.concealMode;
  els.packetLoss.value = t.packetLoss;
  els.packetMs.value = t.packetMs;
  els.repeatMs.value = t.repeatMs;
  els.jitterMs.value = t.jitterMs;
  els.jitterRate.value = t.jitterRate;
  els.gate.value = t.gate;
  els.bits.value = t.bits;
  els.rate.value = t.rate;
  els.ceiling.value = t.ceiling;
  els.outGain.value = t.outGain;
  refreshValueLabels();
}

async function ensureRealtimeGraph() {
  if (realtime.ctx && realtime.graph) return;
  if (!audioBuffer) throw new Error("No audio loaded");
  realtime.ctx = new (window.AudioContext || window.webkitAudioContext)({
    sampleRate: audioBuffer.sampleRate,
    latencyHint: "interactive",
  });
  realtime.seed = audioDataSeed >>> 0;
  realtime.graph = await buildConferenceGraph(realtime.ctx, { seed: realtime.seed });
  realtime.graph.output.connect(realtime.ctx.destination);
  applyRealtimeSettings({ ramp: 0 });
}

function applyRealtimeSettings({ ramp = 0.02 } = {}) {
  if (!realtime.ctx || !realtime.graph) return;
  realtime.graph.applySettings(readSettingsFromUI(), { time: realtime.ctx.currentTime, ramp });
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
  els.stopBtn.disabled = true;
  setState(audioBuffer ? "Ready" : "Idle");
}

async function teardownRealtime() {
  stopPlayback();
  if (realtime.ctx) {
    try {
      await realtime.ctx.close();
    } catch {
      // ignore
    }
  }
  realtime.ctx = null;
  realtime.graph = null;
}

async function startPlayback() {
  if (!audioBuffer) return;
  await ensureRealtimeGraph();
  if (!realtime.ctx || !realtime.graph) return;
  stopPlayback();

  await realtime.ctx.resume();
  applyRealtimeSettings({ ramp: 0.03 });
  realtime.graph.reset(realtime.seed);

  const src = new AudioBufferSourceNode(realtime.ctx, { buffer: audioBuffer });
  src.loop = Boolean(els.loopToggle.checked);
  src.connect(realtime.graph.input);
  src.onended = () => {
    if (realtime.src === src) {
      realtime.src = null;
      els.stopBtn.disabled = true;
      setState("Stopped");
    }
  };
  realtime.src = src;
  src.start();
  els.stopBtn.disabled = false;
  setState("Playing");
}

function downloadBytes(bytes, fileName) {
  const blob = new Blob([bytes], { type: "audio/wav" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = fileName;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

async function exportWav() {
  if (!audioBuffer) return;
  els.exportBtn.disabled = true;
  setState("Rendering...");
  try {
    const sr = audioBuffer.sampleRate;
    const frames = Math.max(1, audioBuffer.length);
    const offline = new OfflineAudioContext(1, frames, sr);
    const graph = await buildConferenceGraph(offline, { seed: audioDataSeed });
    graph.output.connect(offline.destination);
    graph.applySettings(readSettingsFromUI(), { time: 0, ramp: 0 });
    graph.reset(audioDataSeed);

    const src = new AudioBufferSourceNode(offline, { buffer: audioBuffer });
    src.connect(graph.input);
    src.start(0);
    const rendered = await offline.startRendering();

    const wav = encodeWavMono16(rendered);
    const base = (els.fileName.textContent || "bounce").replace(/\.[^/.]+$/, "");
    downloadBytes(wav, `${base}-conference.wav`);
    setState("Exported");
  } catch (e) {
    console.error(e);
    setState("Export failed");
  } finally {
    els.exportBtn.disabled = false;
  }
}

async function loadFile(file) {
  if (!file) return;
  setState("Decoding...");
  await teardownRealtime();
  audioBuffer = null;

  const ab = await file.arrayBuffer();
  audioDataSeed = fnv1a32Sampled(new Uint8Array(ab));
  const decodeCtx = new (window.AudioContext || window.webkitAudioContext)();
  try {
    const decoded = await decodeCtx.decodeAudioData(ab.slice(0));
    const out = new AudioBuffer({ length: decoded.length, sampleRate: decoded.sampleRate, numberOfChannels: 1 });
    const dst = out.getChannelData(0);
    if (decoded.numberOfChannels === 1) dst.set(decoded.getChannelData(0));
    else {
      const a = decoded.getChannelData(0);
      const b = decoded.getChannelData(1);
      for (let i = 0; i < dst.length; i++) dst[i] = 0.5 * (a[i] + b[i]);
    }
    audioBuffer = out;
  } finally {
    try {
      await decodeCtx.close();
    } catch {
      // ignore
    }
  }

  els.fileName.textContent = file.name;
  els.duration.textContent = fmtTime(audioBuffer.duration);
  els.sampleRate.textContent = `${audioBuffer.sampleRate} Hz`;
  els.playBtn.disabled = false;
  els.exportBtn.disabled = false;
  setState("Ready");
}

els.fileInput.addEventListener("change", async () => {
  const file = els.fileInput.files?.[0];
  if (!file) return;
  try {
    await loadFile(file);
  } catch (e) {
    console.error(e);
    setState("Failed to load");
  }
});
els.playBtn.addEventListener("click", () => startPlayback());
els.stopBtn.addEventListener("click", () => stopPlayback());
els.exportBtn.addEventListener("click", () => exportWav());

for (const el of [els.mode, els.bandwidth, els.codec, els.dropouts, els.jitter, els.robot, els.noise]) {
  el.addEventListener("input", () => {
    applyMacrosFromPrimaryToAdvanced();
    settings = readSettingsFromUI();
    applyRealtimeSettings();
    els.preset.value = "";
  });
  el.addEventListener("change", () => {
    applyMacrosFromPrimaryToAdvanced();
    settings = readSettingsFromUI();
    applyRealtimeSettings();
    els.preset.value = "";
  });
}

for (const el of [
  els.hpHz,
  els.lpHz,
  els.midHumpDb,
  els.midFreq,
  els.concealMode,
  els.packetLoss,
  els.packetMs,
  els.repeatMs,
  els.jitterMs,
  els.jitterRate,
  els.gate,
  els.bits,
  els.rate,
  els.ceiling,
  els.outGain,
]) {
  el.addEventListener("input", () => {
    settings = readSettingsFromUI();
    refreshValueLabels();
    applyRealtimeSettings();
    els.preset.value = "";
  });
  el.addEventListener("change", () => {
    settings = readSettingsFromUI();
    refreshValueLabels();
    applyRealtimeSettings();
    els.preset.value = "";
  });
}

function populatePresets() {
  els.preset.replaceChildren();
  const none = document.createElement("option");
  none.value = "";
  none.textContent = "Custom";
  els.preset.appendChild(none);
  for (const [id, p] of Object.entries(PRESETS)) {
    const opt = document.createElement("option");
    opt.value = id;
    opt.textContent = p.name || id;
    els.preset.appendChild(opt);
  }
}

els.preset.addEventListener("change", () => {
  const key = els.preset.value;
  if (!key) return;
  const p = PRESETS[key];
  if (!p) return;
  const merged = { ...defaultSettings(), ...p };
  writeSettingsToUI(merged);
  applyMacrosFromPrimaryToAdvanced();
  settings = readSettingsFromUI();
  applyRealtimeSettings({ ramp: 0.02 });
});

function init() {
  populatePresets();
  writeSettingsToUI(settings);
  applyMacrosFromPrimaryToAdvanced();
  els.fileName.textContent = "None";
  els.duration.textContent = "-";
  els.sampleRate.textContent = "-";
  setState("Idle");
}

init();
