import { buildTransmissionGraph, defaultSettings, mapBandwidth } from "./audio/graph.js";
import { encodeWavMono16 } from "./audio/wav.js";
import { PRESETS } from "./presets.js";

const els = {
  fileInput: document.querySelector("#fileInput"),
  playBtn: document.querySelector("#playBtn"),
  stopBtn: document.querySelector("#stopBtn"),
  exportBtn: document.querySelector("#exportBtn"),
  loopToggle: document.querySelector("#loopToggle"),

  bandwidth: document.querySelector("#bandwidth"),
  drive: document.querySelector("#drive"),
  badConnection: document.querySelector("#badConnection"),
  noiseProfile: document.querySelector("#noiseProfile"),
  walkieMode: document.querySelector("#walkieMode"),
  pinkNoise: document.querySelector("#pinkNoise"),
  preset: document.querySelector("#preset"),
  presetName: document.querySelector("#presetName"),
  savePresetBtn: document.querySelector("#savePresetBtn"),
  deletePresetBtn: document.querySelector("#deletePresetBtn"),

  hpHz: document.querySelector("#hpHz"),
  lpHz: document.querySelector("#lpHz"),
  midGainDb: document.querySelector("#midGainDb"),
  midFreq: document.querySelector("#midFreq"),
  midQ: document.querySelector("#midQ"),
  boxDipDb: document.querySelector("#boxDipDb"),

  comp: document.querySelector("#comp"),
  asym: document.querySelector("#asym"),
  preDrive: document.querySelector("#preDrive"),
  postDrive: document.querySelector("#postDrive"),
  crush: document.querySelector("#crush"),

  wowDepth: document.querySelector("#wowDepth"),
  dropRate: document.querySelector("#dropRate"),
  dropDepth: document.querySelector("#dropDepth"),
  crackle: document.querySelector("#crackle"),
  lfoRate: document.querySelector("#lfoRate"),

  noiseColor: document.querySelector("#noiseColor"),
  hiss: document.querySelector("#hiss"),

  walkieThresholdDb: document.querySelector("#walkieThresholdDb"),
  walkieMinSilenceMs: document.querySelector("#walkieMinSilenceMs"),
  walkieClickMs: document.querySelector("#walkieClickMs"),
  walkieClickLevel: document.querySelector("#walkieClickLevel"),
  walkieFx: document.querySelector("#walkieFx"),

  tuningEnable: document.querySelector("#tuningEnable"),
  tuningMode: document.querySelector("#tuningMode"),
  tuningSource: document.querySelector("#tuningSource"),
  tuningAmount: document.querySelector("#tuningAmount"),
  tuningSnippetMs: document.querySelector("#tuningSnippetMs"),
  tuningCutDepth: document.querySelector("#tuningCutDepth"),

  outGain: document.querySelector("#outGain"),
  passes: document.querySelector("#passes"),

  bandwidthVal: document.querySelector("#bandwidthVal"),
  driveVal: document.querySelector("#driveVal"),
  badConnectionVal: document.querySelector("#badConnectionVal"),
  noiseProfileVal: document.querySelector("#noiseProfileVal"),

  hpHzVal: document.querySelector("#hpHzVal"),
  lpHzVal: document.querySelector("#lpHzVal"),
  midGainDbVal: document.querySelector("#midGainDbVal"),
  midFreqVal: document.querySelector("#midFreqVal"),
  midQVal: document.querySelector("#midQVal"),
  boxDipDbVal: document.querySelector("#boxDipDbVal"),

  compVal: document.querySelector("#compVal"),
  asymVal: document.querySelector("#asymVal"),
  preDriveVal: document.querySelector("#preDriveVal"),
  postDriveVal: document.querySelector("#postDriveVal"),
  crushVal: document.querySelector("#crushVal"),

  wowDepthVal: document.querySelector("#wowDepthVal"),
  dropRateVal: document.querySelector("#dropRateVal"),
  dropDepthVal: document.querySelector("#dropDepthVal"),
  crackleVal: document.querySelector("#crackleVal"),
  lfoRateVal: document.querySelector("#lfoRateVal"),

  noiseColorVal: document.querySelector("#noiseColorVal"),
  hissVal: document.querySelector("#hissVal"),

  walkieThresholdDbVal: document.querySelector("#walkieThresholdDbVal"),
  walkieMinSilenceMsVal: document.querySelector("#walkieMinSilenceMsVal"),
  walkieClickMsVal: document.querySelector("#walkieClickMsVal"),
  walkieClickLevelVal: document.querySelector("#walkieClickLevelVal"),
  walkieFxVal: document.querySelector("#walkieFxVal"),

  tuningEnableVal: document.querySelector("#tuningEnableVal"),
  tuningModeVal: document.querySelector("#tuningModeVal"),
  tuningSourceVal: document.querySelector("#tuningSourceVal"),
  tuningAmountVal: document.querySelector("#tuningAmountVal"),
  tuningSnippetMsVal: document.querySelector("#tuningSnippetMsVal"),
  tuningCutDepthVal: document.querySelector("#tuningCutDepthVal"),

  outGainVal: document.querySelector("#outGainVal"),
  passesVal: document.querySelector("#passesVal"),

  fileName: document.querySelector("#fileName"),
  duration: document.querySelector("#duration"),
  sampleRate: document.querySelector("#sampleRate"),
  state: document.querySelector("#state"),
};

function fmtTime(seconds) {
  if (!Number.isFinite(seconds)) return "-";
  const m = Math.floor(seconds / 60);
  const s = Math.floor(seconds % 60);
  return `${m}:${String(s).padStart(2, "0")}`;
}

function clamp01(x) {
  return Math.min(1, Math.max(0, x));
}

function round1(x) {
  return Math.round(x * 10) / 10;
}

function pct01(x) {
  return `${Math.round(clamp01(x) * 100)}%`;
}

function fnv1a32Sampled(uint8) {
  let h = 0x811c9dc5;
  const maxOps = 1_000_000;
  const stride = Math.max(1, Math.floor(uint8.length / maxOps));
  for (let i = 0; i < uint8.length; i += stride) {
    h ^= uint8[i];
    h = Math.imul(h, 0x01000193);
  }
  return h >>> 0;
}

function setState(text) {
  els.state.textContent = text;
}

let settings = defaultSettings();
let audioBuffer = null;
let audioDataSeed = 0xdecafbad;
let selectedPresetMeta = { kind: "builtin", id: "" };
let tuningEdges = null;

let realtime = {
  ctx: null,
  graph: null,
  src: null,
  playing: false,
};

const STORAGE_KEY = "transmission_engine_presets_v1";
const TUNING_MANIFEST_URL = new URL("../audio/manifest.json", import.meta.url);

let decodeCtx = null;
function getDecodeCtx() {
  if (decodeCtx) return decodeCtx;
  decodeCtx = new (window.AudioContext || window.webkitAudioContext)();
  return decodeCtx;
}

const tuningSampleCache = new Map();

async function loadTuningManifest() {
  try {
    const res = await fetch(TUNING_MANIFEST_URL, { cache: "no-cache" });
    if (!res.ok) return [];
    const json = await res.json();
    if (Array.isArray(json)) {
      return json
        .map((s, i) => ({ id: s.id ?? `sample${i}`, name: s.name ?? s.file ?? String(s), file: s.file ?? String(s) }))
        .filter((s) => typeof s.file === "string");
    }
    const arr = json?.samples;
    if (!Array.isArray(arr)) return [];
    return arr
      .map((s, i) => {
        if (typeof s === "string") return { id: `sample${i}`, name: s, file: s };
        return { id: s.id ?? `sample${i}`, name: s.name ?? s.file ?? `Sample ${i + 1}`, file: s.file };
      })
      .filter((s) => s && typeof s.file === "string");
  } catch {
    return [];
  }
}

function setTuningSourceOptions(samples) {
  if (!els.tuningSource) return;
  const prev = els.tuningSource.value || "synth";
  els.tuningSource.replaceChildren();
  const synth = document.createElement("option");
  synth.value = "synth";
  synth.textContent = "Synth";
  els.tuningSource.appendChild(synth);
  for (const s of samples) {
    const opt = document.createElement("option");
    opt.value = `file:${s.file}`;
    opt.textContent = s.name;
    els.tuningSource.appendChild(opt);
  }
  els.tuningSource.value = Array.from(els.tuningSource.options).some((o) => o.value === prev) ? prev : "synth";
}

async function loadTuningSample(sourceValue) {
  if (!sourceValue || sourceValue === "synth" || !sourceValue.startsWith("file:")) return null;
  const file = sourceValue.slice("file:".length);
  const urlObj = new URL(`../audio/${encodeURIComponent(file)}`, import.meta.url);
  const url = urlObj.href;
  if (tuningSampleCache.has(url)) return tuningSampleCache.get(url);

  const res = await fetch(url);
  if (!res.ok) throw new Error(`Failed to fetch tuning sample: ${url}`);
  const buf = await res.arrayBuffer();
  const ctx = getDecodeCtx();
  const decoded = await ctx.decodeAudioData(buf.slice(0));
  const mono = new Float32Array(decoded.length);
  if (decoded.numberOfChannels === 1) mono.set(decoded.getChannelData(0));
  else {
    const a = decoded.getChannelData(0);
    const b = decoded.getChannelData(1);
    for (let i = 0; i < mono.length; i++) mono[i] = 0.5 * (a[i] + b[i]);
  }
  const sample = { sampleRate: decoded.sampleRate, data: mono };
  tuningSampleCache.set(url, sample);
  return sample;
}

function loadUserPresets() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return [];
    const arr = JSON.parse(raw);
    if (!Array.isArray(arr)) return [];
    return arr.filter((p) => p && typeof p.id === "string" && typeof p.name === "string" && p.settings && typeof p.settings === "object");
  } catch {
    return [];
  }
}

function saveUserPresets(presets) {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(presets));
}

function ensureSavedOptGroup() {
  const select = els.preset;
  let og = select.querySelector('optgroup[label="Saved"]');
  if (!og) {
    og = document.createElement("optgroup");
    og.label = "Saved";
    select.appendChild(og);
  }
  return og;
}

function refreshPresetDropdown() {
  const og = ensureSavedOptGroup();
  og.replaceChildren();
  const presets = loadUserPresets();
  for (const p of presets) {
    const opt = document.createElement("option");
    opt.value = `user:${p.id}`;
    opt.textContent = p.name;
    og.appendChild(opt);
  }
}

function stableIdFromName(name) {
  const base = name.trim().toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "");
  const salt = String(Date.now()).slice(-6);
  return `${base || "preset"}-${salt}`;
}

function readSettingsFromUI() {
  settings = {
    bandwidth: clamp01(Number(els.bandwidth.value)),
    drive: clamp01(Number(els.drive.value)),
    badConnection: clamp01(Number(els.badConnection.value)),
    noiseProfile: clamp01(Number(els.noiseProfile.value)),
    walkieMode: Boolean(els.walkieMode.checked),
    pinkNoise: Boolean(els.pinkNoise.checked),

    hpHz: Number(els.hpHz.value),
    lpHz: Number(els.lpHz.value),
    midGainDb: Number(els.midGainDb.value),
    midFreq: Number(els.midFreq.value),
    midQ: Number(els.midQ.value),
    boxDipDb: Number(els.boxDipDb.value),

    comp: clamp01(Number(els.comp.value)),
    asym: clamp01(Number(els.asym.value)),
    preDrive: clamp01(Number(els.preDrive.value)),
    postDrive: clamp01(Number(els.postDrive.value)),
    crush: clamp01(Number(els.crush.value)),

    wowDepth: clamp01(Number(els.wowDepth.value)),
    dropRate: clamp01(Number(els.dropRate.value)),
    dropDepth: clamp01(Number(els.dropDepth.value)),
    crackle: clamp01(Number(els.crackle.value)),
    lfoRate: Number(els.lfoRate.value),

    noiseColor: clamp01(Number(els.noiseColor.value)),
    hiss: clamp01(Number(els.hiss.value)),

    walkieThresholdDb: Number(els.walkieThresholdDb.value),
    walkieMinSilenceMs: Number(els.walkieMinSilenceMs.value),
    walkieClickMs: Number(els.walkieClickMs.value),
    walkieClickLevel: clamp01(Number(els.walkieClickLevel.value)),
    walkieFx: els.walkieFx?.value === "dispatch" ? "dispatch" : "click",

    tuningEnable: Boolean(els.tuningEnable?.checked),
    tuningMode: els.tuningMode?.value === "search" ? "search" : "edges",
    tuningSource: els.tuningSource?.value || "synth",
    tuningAmount: clamp01(Number(els.tuningAmount?.value ?? 0.35)),
    tuningSnippetMs: Number(els.tuningSnippetMs?.value ?? 140),
    tuningCutDepth: clamp01(Number(els.tuningCutDepth?.value ?? 0.55)),

    outGain: Number(els.outGain.value),
    passes: Math.max(1, Math.min(6, Math.floor(Number(els.passes.value)))),
  };
  return settings;
}

function refreshValueLabels() {
  const s = readSettingsFromUI();

  els.bandwidthVal.textContent = `HP ${Math.round(s.hpHz)}Hz / LP ${round1(s.lpHz / 1000)}kHz`;
  els.driveVal.textContent = pct01(s.drive);
  els.badConnectionVal.textContent = pct01(s.badConnection);
  els.noiseProfileVal.textContent = pct01(s.noiseProfile);

  els.hpHzVal.textContent = `${Math.round(s.hpHz)} Hz`;
  els.lpHzVal.textContent = `${round1(s.lpHz / 1000)} kHz`;
  els.midGainDbVal.textContent = `+${round1(s.midGainDb)} dB`;
  els.midFreqVal.textContent = `${Math.round(s.midFreq)} Hz`;
  els.midQVal.textContent = `${round1(s.midQ)}`;
  els.boxDipDbVal.textContent = `-${round1(s.boxDipDb)} dB`;

  els.compVal.textContent = pct01(s.comp);
  els.asymVal.textContent = pct01(s.asym);
  els.preDriveVal.textContent = pct01(s.preDrive);
  els.postDriveVal.textContent = pct01(s.postDrive);
  els.crushVal.textContent = pct01(s.crush);

  els.wowDepthVal.textContent = pct01(s.wowDepth);
  els.dropRateVal.textContent = pct01(s.dropRate);
  els.dropDepthVal.textContent = pct01(s.dropDepth);
  els.crackleVal.textContent = pct01(s.crackle);
  els.lfoRateVal.textContent = `${round1(s.lfoRate)} Hz`;

  els.noiseColorVal.textContent = s.noiseColor >= 0.5 ? `Pink ${pct01(s.noiseColor)}` : `White ${pct01(1 - s.noiseColor)}`;
  els.hissVal.textContent = pct01(s.hiss);

  els.walkieThresholdDbVal.textContent = `${Math.round(s.walkieThresholdDb)} dB`;
  els.walkieMinSilenceMsVal.textContent = `${Math.round(s.walkieMinSilenceMs)} ms`;
  els.walkieClickMsVal.textContent = `${Math.round(s.walkieClickMs)} ms`;
  els.walkieClickLevelVal.textContent = pct01(s.walkieClickLevel);
  els.walkieFxVal.textContent = s.walkieFx === "dispatch" ? "beep" : "click";

  els.tuningEnableVal.textContent = s.tuningEnable ? "on" : "off";
  els.tuningModeVal.textContent = s.tuningMode === "search" ? "search" : "edges";
  els.tuningSourceVal.textContent = s.tuningSource === "synth" ? "synth" : "file";
  els.tuningAmountVal.textContent = pct01(s.tuningAmount);
  els.tuningSnippetMsVal.textContent = `${Math.round(s.tuningSnippetMs)} ms`;
  els.tuningCutDepthVal.textContent = pct01(s.tuningCutDepth);

  els.outGainVal.textContent = `${round1(s.outGain)}x`;
  els.passesVal.textContent = `${s.passes}x`;
}

function writeSettingsToUI(next) {
  els.bandwidth.value = String(next.bandwidth ?? 0.45);
  els.drive.value = String(next.drive ?? 0.35);
  els.badConnection.value = String(next.badConnection ?? 0.25);
  els.noiseProfile.value = String(next.noiseProfile ?? 0.2);
  els.walkieMode.checked = Boolean(next.walkieMode);
  els.pinkNoise.checked = Boolean(next.pinkNoise);

  els.hpHz.value = String(next.hpHz ?? 380);
  els.lpHz.value = String(next.lpHz ?? 5200);
  els.midGainDb.value = String(next.midGainDb ?? 0);
  els.midFreq.value = String(next.midFreq ?? 1550);
  els.midQ.value = String(next.midQ ?? 1.2);
  els.boxDipDb.value = String(next.boxDipDb ?? 0);

  els.comp.value = String(next.comp ?? 0.25);
  els.asym.value = String(next.asym ?? 0.1);
  els.preDrive.value = String(next.preDrive ?? 0.25);
  els.postDrive.value = String(next.postDrive ?? (next.drive ?? 0.35));
  els.crush.value = String(next.crush ?? 0);

  els.wowDepth.value = String(next.wowDepth ?? 0.25);
  els.dropRate.value = String(next.dropRate ?? 0.25);
  els.dropDepth.value = String(next.dropDepth ?? 0.35);
  els.crackle.value = String(next.crackle ?? 0.25);
  els.lfoRate.value = String(next.lfoRate ?? 0.7);

  els.noiseColor.value = String(next.noiseColor ?? 0);
  els.hiss.value = String(next.hiss ?? 0.2);

  els.walkieThresholdDb.value = String(next.walkieThresholdDb ?? -45);
  els.walkieMinSilenceMs.value = String(next.walkieMinSilenceMs ?? 220);
  els.walkieClickMs.value = String(next.walkieClickMs ?? 12);
  els.walkieClickLevel.value = String(next.walkieClickLevel ?? 0.65);
  if (els.walkieFx) els.walkieFx.value = next.walkieFx === "dispatch" ? "dispatch" : "click";

  if (els.tuningEnable) els.tuningEnable.checked = Boolean(next.tuningEnable);
  if (els.tuningMode) els.tuningMode.value = next.tuningMode === "search" ? "search" : "edges";
  if (els.tuningSource) els.tuningSource.value = next.tuningSource ?? "synth";
  if (els.tuningAmount) els.tuningAmount.value = String(next.tuningAmount ?? 0.35);
  if (els.tuningSnippetMs) els.tuningSnippetMs.value = String(next.tuningSnippetMs ?? 140);
  if (els.tuningCutDepth) els.tuningCutDepth.value = String(next.tuningCutDepth ?? 0.55);

  els.outGain.value = String(next.outGain ?? 0.92);
  els.passes.value = String(next.passes ?? 1);

  refreshValueLabels();
}

function applyBandwidthMacro(bw) {
  const { hp, lp, midGainDb, midQ, midFreq } = mapBandwidth(bw);
  els.hpHz.value = String(Math.round(hp));
  els.lpHz.value = String(Math.round(lp));
  els.midGainDb.value = String(midGainDb);
  els.midQ.value = String(midQ);
  els.midFreq.value = String(midFreq);
  els.boxDipDb.value = String((1 - bw) * 2.2);
}

function applyDriveMacro(drive) {
  els.asym.value = String(drive * 0.6);
  els.comp.value = String(0.18 + drive * 0.65);
  els.preDrive.value = String(Math.pow(drive, 0.85) * 0.75);
  els.postDrive.value = String(drive);
}

function applyBadMacro(bad) {
  els.wowDepth.value = String(bad);
  els.dropRate.value = String(bad);
  els.dropDepth.value = String(bad);
  els.crackle.value = String(bad);
  els.lfoRate.value = String(0.45 + bad * 1.6);
}

function applyNoiseMacro(noiseProfile) {
  els.hiss.value = String(noiseProfile * 0.95);
  const autoColor = Math.max(0, (noiseProfile - 0.55) * 2);
  els.noiseColor.value = String(els.pinkNoise.checked ? 1 : autoColor);
}

function syncPinkToggle() {
  const v = clamp01(Number(els.noiseColor.value));
  els.pinkNoise.checked = v >= 0.5;
}

function dbToLin(db) {
  return Math.pow(10, db / 20);
}

function computeLeadingTrailingSilenceEdges(audioBuffer, { thresholdDb = -50 } = {}) {
  const ch = audioBuffer.getChannelData(0);
  const thr = dbToLin(thresholdDb);
  const window = Math.max(256, Math.floor(audioBuffer.sampleRate * 0.012));
  const hop = Math.max(128, Math.floor(window / 2));

  const rmsAt = (start) => {
    let sum = 0;
    const end = Math.min(ch.length, start + window);
    for (let i = start; i < end; i++) sum += ch[i] * ch[i];
    const n = Math.max(1, end - start);
    return Math.sqrt(sum / n);
  };

  let leadEnd = 0;
  for (let i = 0; i < ch.length; i += hop) {
    if (rmsAt(i) > thr) {
      leadEnd = Math.max(0, i);
      break;
    }
  }

  let tailStart = ch.length;
  for (let i = Math.max(0, ch.length - window); i > 0; i -= hop) {
    if (rmsAt(i) > thr) {
      tailStart = Math.min(ch.length, i + window);
      break;
    }
  }

  if (tailStart < leadEnd) tailStart = leadEnd;
  return { leadEnd, tailStart };
}

async function ensureRealtimeGraph() {
  if (!audioBuffer) return;
  const s = readSettingsFromUI();
  const desiredPasses = s.passes ?? 1;
  if (realtime.ctx && realtime.ctx.sampleRate !== audioBuffer.sampleRate) {
    stopPlayback();
    await realtime.ctx.close();
    realtime.ctx = null;
    realtime.graph = null;
  }
  if (realtime.ctx && realtime.graph) return;
  realtime.ctx = new (window.AudioContext || window.webkitAudioContext)({
    latencyHint: "interactive",
    sampleRate: audioBuffer.sampleRate,
  });
  let tuningSample = null;
  try {
    if (s.tuningEnable && s.tuningSource && s.tuningSource !== "synth") tuningSample = await loadTuningSample(s.tuningSource);
  } catch (e) {
    console.warn(e);
  }
  realtime.graph = await buildTransmissionGraph(realtime.ctx, {
    seed: audioDataSeed,
    passes: desiredPasses,
    tuningEdges,
    tuningSample,
  });
  realtime.graph.output.connect(realtime.ctx.destination);
  realtime.graph.applySettings(s, { ramp: 0.03 });
}

function stopPlayback() {
  if (!realtime.playing) return;
  try {
    realtime.src?.stop();
  } catch {
    // ignore
  }
  realtime.src?.disconnect();
  realtime.src = null;
  realtime.playing = false;
  els.playBtn.disabled = !audioBuffer;
  els.stopBtn.disabled = true;
  setState("Idle");
}

async function startPlayback() {
  if (!audioBuffer) return;
  await ensureRealtimeGraph();
  await realtime.ctx.resume();

  stopPlayback();

  realtime.graph.reset(audioDataSeed);
  realtime.graph.applySettings(readSettingsFromUI(), { ramp: 0 });

  const src = new AudioBufferSourceNode(realtime.ctx, { buffer: audioBuffer });
  src.loop = Boolean(els.loopToggle?.checked);
  src.loopStart = 0;
  src.loopEnd = audioBuffer.duration;
  realtime.src = src;
  src.connect(realtime.graph.input);
  src.onended = () => {
    if (realtime.src !== src) return;
    stopPlayback();
  };
  realtime.playing = true;
  els.playBtn.disabled = true;
  els.stopBtn.disabled = false;
  setState("Playing");
  src.start();
}

async function exportWav() {
  if (!audioBuffer) return;
  els.exportBtn.disabled = true;
  setState("Rendering...");

  const sampleRate = audioBuffer.sampleRate;
  const s = readSettingsFromUI();
  const offline = new OfflineAudioContext({
    numberOfChannels: 1,
    length: audioBuffer.length,
    sampleRate,
  });
  let tuningSample = null;
  try {
    if (s.tuningEnable && s.tuningSource && s.tuningSource !== "synth") tuningSample = await loadTuningSample(s.tuningSource);
  } catch (e) {
    console.warn(e);
  }

  const graph = await buildTransmissionGraph(offline, { seed: audioDataSeed, passes: s.passes ?? 1, tuningEdges, tuningSample });
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
  a.download = `${base} - Transmission Engine.wav`;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 5000);

  setState("Exported");
  els.exportBtn.disabled = false;
}

function applyToGraphAndMarkCustom() {
  refreshValueLabels();
  if (realtime.graph && realtime.ctx) realtime.graph.applySettings(settings, { ramp: 0.03 });
  els.preset.value = "";
}

function hookControls() {
  els.bandwidth.addEventListener("input", () => {
    applyBandwidthMacro(clamp01(Number(els.bandwidth.value)));
    applyToGraphAndMarkCustom();
  });
  els.drive.addEventListener("input", () => {
    applyDriveMacro(clamp01(Number(els.drive.value)));
    applyToGraphAndMarkCustom();
  });
  els.badConnection.addEventListener("input", () => {
    applyBadMacro(clamp01(Number(els.badConnection.value)));
    applyToGraphAndMarkCustom();
  });
  els.noiseProfile.addEventListener("input", () => {
    applyNoiseMacro(clamp01(Number(els.noiseProfile.value)));
    syncPinkToggle();
    applyToGraphAndMarkCustom();
  });

  els.walkieMode.addEventListener("change", () => applyToGraphAndMarkCustom());
  els.pinkNoise.addEventListener("change", () => {
    els.noiseColor.value = els.pinkNoise.checked ? "1" : "0";
    applyToGraphAndMarkCustom();
  });

  const advanced = [
    els.hpHz,
    els.lpHz,
    els.midGainDb,
    els.midFreq,
    els.midQ,
    els.boxDipDb,
    els.comp,
    els.asym,
    els.preDrive,
    els.postDrive,
    els.crush,
    els.wowDepth,
    els.dropRate,
    els.dropDepth,
    els.crackle,
    els.lfoRate,
    els.noiseColor,
    els.hiss,
    els.walkieThresholdDb,
    els.walkieMinSilenceMs,
    els.walkieClickMs,
    els.walkieClickLevel,
    els.walkieFx,
    els.tuningEnable,
    els.tuningMode,
    els.tuningSource,
    els.tuningAmount,
    els.tuningSnippetMs,
    els.tuningCutDepth,
    els.outGain,
    els.passes,
  ];
  for (const el of advanced) {
    if (el === els.passes) continue;
    el.addEventListener("input", () => {
      if (el === els.noiseColor) syncPinkToggle();
      applyToGraphAndMarkCustom();
    });
  }

  els.tuningEnable?.addEventListener("change", () => applyToGraphAndMarkCustom());
  els.tuningMode?.addEventListener("change", () => applyToGraphAndMarkCustom());
  els.tuningSource?.addEventListener("change", async () => {
    refreshValueLabels();
    const s = readSettingsFromUI();
    if (realtime.graph?.nodes?.tuningNode) {
      try {
        if (s.tuningSource && s.tuningSource !== "synth") {
          const sample = await loadTuningSample(s.tuningSource);
          if (sample) realtime.graph.nodes.tuningNode.port.postMessage({ type: "setSample", sampleRate: sample.sampleRate, data: sample.data });
        }
      } catch (e) {
        console.warn(e);
      }
      realtime.graph.applySettings(s, { ramp: 0.03 });
    }
    els.preset.value = "";
  });

  els.passes.addEventListener("input", async () => {
    const wasPlaying = realtime.playing;
    stopPlayback();
    if (realtime.graph) {
      try {
        realtime.graph.output.disconnect();
      } catch {
        // ignore
      }
    }
    realtime.graph = null;
    refreshValueLabels();
    if (audioBuffer) await ensureRealtimeGraph();
    if (wasPlaying) await startPlayback();
    els.preset.value = "";
  });

  els.preset.addEventListener("change", () => {
    const key = els.preset.value;
    if (!key) return;
    let preset = null;
    if (key.startsWith("user:")) {
      const id = key.slice("user:".length);
      const saved = loadUserPresets().find((p) => p.id === id);
      if (saved) {
        preset = saved.settings;
        selectedPresetMeta = { kind: "user", id };
        els.deletePresetBtn.disabled = false;
        els.presetName.value = saved.name;
      }
    } else {
      preset = PRESETS[key];
      selectedPresetMeta = { kind: "builtin", id: key };
      els.deletePresetBtn.disabled = true;
      els.presetName.value = "";
    }
    if (!preset) return;

    writeSettingsToUI({ ...defaultSettings(), ...preset });
    const s = readSettingsFromUI();
    const needsRebuild = realtime.graph && realtime.graph.passes !== (s.passes ?? 1);
    if (needsRebuild) {
      const wasPlaying = realtime.playing;
      stopPlayback();
      if (realtime.graph) {
        try {
          realtime.graph.output.disconnect();
        } catch {
          // ignore
        }
      }
      realtime.graph = null;
      if (audioBuffer) ensureRealtimeGraph().then(() => (wasPlaying ? startPlayback() : null));
    } else if (realtime.graph && realtime.ctx) {
      realtime.graph.applySettings(s, { ramp: 0.03 });
    }
  });

  els.savePresetBtn?.addEventListener("click", () => {
    const name = (els.presetName?.value || "").trim();
    if (!name) return;
    const s = readSettingsFromUI();
    const presets = loadUserPresets();
    if (selectedPresetMeta.kind === "user") {
      const idx = presets.findIndex((p) => p.id === selectedPresetMeta.id);
      if (idx >= 0) presets[idx] = { ...presets[idx], name, settings: s };
      else presets.push({ id: selectedPresetMeta.id, name, settings: s });
    } else {
      const id = stableIdFromName(name);
      presets.push({ id, name, settings: s });
      selectedPresetMeta = { kind: "user", id };
      els.deletePresetBtn.disabled = false;
    }
    saveUserPresets(presets);
    refreshPresetDropdown();
    els.preset.value = `user:${selectedPresetMeta.id}`;
  });

  els.deletePresetBtn?.addEventListener("click", () => {
    if (selectedPresetMeta.kind !== "user") return;
    const presets = loadUserPresets().filter((p) => p.id !== selectedPresetMeta.id);
    saveUserPresets(presets);
    selectedPresetMeta = { kind: "builtin", id: "" };
    els.deletePresetBtn.disabled = true;
    els.presetName.value = "";
    refreshPresetDropdown();
    els.preset.value = "";
  });
}

function wireButtons() {
  els.playBtn.addEventListener("click", () => startPlayback());
  els.stopBtn.addEventListener("click", () => stopPlayback());
  els.exportBtn.addEventListener("click", () => exportWav());
  els.loopToggle?.addEventListener("change", () => {
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
    tuningEdges = computeLeadingTrailingSilenceEdges(mono, { thresholdDb: -50 });
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
    }
    if (realtime.graph?.nodes?.tuningNode && tuningEdges) {
      realtime.graph.nodes.tuningNode.port.postMessage({ type: "setEdges", leadEnd: tuningEdges.leadEnd, tailStart: tuningEdges.tailStart });
    }
    els.fileName.textContent = file.name;
    els.duration.textContent = fmtTime(audioBuffer.duration);
    els.sampleRate.textContent = `${audioBuffer.sampleRate} Hz`;
    setState("Ready");
    els.playBtn.disabled = false;
    els.exportBtn.disabled = false;
  } catch (err) {
    console.error(err);
    audioBuffer = null;
    setState("Decode failed");
    els.fileName.textContent = "None";
    els.duration.textContent = "-";
    els.sampleRate.textContent = "-";
  }
});

hookControls();
wireButtons();

refreshPresetDropdown();
loadTuningManifest().then(setTuningSourceOptions);
writeSettingsToUI(settings);
applyBandwidthMacro(clamp01(Number(els.bandwidth.value)));
applyDriveMacro(clamp01(Number(els.drive.value)));
applyBadMacro(clamp01(Number(els.badConnection.value)));
applyNoiseMacro(clamp01(Number(els.noiseProfile.value)));
syncPinkToggle();
refreshValueLabels();
setState("Idle");
