import { buildLameGraph, buildMasterLane } from "./audio/graph.js";
import { encodeWavPcm16 } from "./audio/wav.js";
import { mapBandwidth } from "../../src/audio/graph.js";
import { ENGINE_PRESETS, MASTER_PRESETS } from "./presets.js";

const TUNING_MANIFEST_URL = new URL("../../audio/manifest.json", import.meta.url);
const TAPE_SFX_MANIFEST_URL = new URL("../../tape-engine/audio/manifest.json", import.meta.url);
const CAMCORDER_SFX_MANIFEST_URL = new URL("../../camcorder-engine/audio/manifest.json", import.meta.url);
const TV_SFX_MANIFEST_URL = new URL("../../television-engine/audio/manifest.json", import.meta.url);

const els = {
  fileInput: document.querySelector("#fileInput"),
  playBtn: document.querySelector("#playBtn"),
  stopBtn: document.querySelector("#stopBtn"),
  exportBtn: document.querySelector("#exportBtn"),
  loopToggle: document.querySelector("#loopToggle"),
  masterPreset: document.querySelector("#masterPreset"),
  viewSelect: document.querySelector("#viewSelect"),
  saveMasterPresetBtn: document.querySelector("#saveMasterPresetBtn"),
  deleteMasterPresetBtn: document.querySelector("#deleteMasterPresetBtn"),

  masterGain: document.querySelector("#masterGain"),
  masterHpHz: document.querySelector("#masterHpHz"),
  masterLpHz: document.querySelector("#masterLpHz"),
  masterComp: document.querySelector("#masterComp"),
  ceiling: document.querySelector("#ceiling"),
  limiter: document.querySelector("#limiter"),
  softClip: document.querySelector("#softClip"),
  monoOut: document.querySelector("#monoOut"),

  masterGainVal: document.querySelector("#masterGainVal"),
  masterHpHzVal: document.querySelector("#masterHpHzVal"),
  masterLpHzVal: document.querySelector("#masterLpHzVal"),
  masterCompVal: document.querySelector("#masterCompVal"),
  ceilingVal: document.querySelector("#ceilingVal"),
  limiterVal: document.querySelector("#limiterVal"),
  optsVal: document.querySelector("#optsVal"),

  fileName: document.querySelector("#fileName"),
  duration: document.querySelector("#duration"),
  sampleRate: document.querySelector("#sampleRate"),
  state: document.querySelector("#state"),

  moduleRow: document.querySelector("#moduleRow"),

  automationView: document.querySelector("#automationView"),
  automationTracks: document.querySelector("#automationTracks"),
  autoBpm: document.querySelector("#autoBpm"),
  autoBpmVal: document.querySelector("#autoBpmVal"),
  autoZoom: document.querySelector("#autoZoom"),
  autoZoomVal: document.querySelector("#autoZoomVal"),
  autoSnap: document.querySelector("#autoSnap"),
  autoLenText: document.querySelector("#autoLenText"),
  autoTarget: document.querySelector("#autoTarget"),
  autoLaneCanvas: document.querySelector("#autoLaneCanvas"),
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

function fnv1a32Sampled(uint8) {
  let h = 0x811c9dc5 >>> 0;
  const step = Math.max(1, Math.floor(uint8.length / 16384));
  for (let i = 0; i < uint8.length; i += step) {
    h ^= uint8[i];
    h = Math.imul(h, 0x01000193) >>> 0;
  }
  h ^= uint8.length >>> 0;
  h = Math.imul(h, 0x01000193) >>> 0;
  return h >>> 0;
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

function presetLabelFromKey(key) {
  const raw = String(key || "");
  if (!raw) return "";
  if (raw.startsWith("user:")) return raw.slice("user:".length);
  const spaced = raw
    .replace(/[_-]+/g, " ")
    .replace(/([a-z])([A-Z0-9])/g, "$1 $2")
    .replace(/([0-9])([a-zA-Z])/g, "$1 $2");
  const words = spaced.split(/\s+/).filter(Boolean);
  return words
    .map((w) => {
      const up = w.toUpperCase();
      if (up === "AM" || up === "FM" || up === "VHS" || up === "IR" || up === "CD" || up === "PA" || up === "VOIP" || up === "WW2") return up;
      if (w.length <= 2 && /[0-9]/.test(w)) return w;
      return w.charAt(0).toUpperCase() + w.slice(1);
    })
    .join(" ");
}

let tuningManifest = { samples: [] };
const tuningSampleCache = new Map(); // url -> {sampleRate,data}

let tapeSfxManifest = { banks: [] };
const tapeSfxCache = new Map(); // filename -> AudioBuffer (mono)

let camcorderSfxManifest = { camBed: [], windBed: [], windHits: [] };
const camcorderSfxCache = new Map(); // filename -> AudioBuffer (mono)

let tvSfxManifest = { beds: [] };
const tvSfxCache = new Map(); // filename -> AudioBuffer (mono)

const LS_ENGINE_PRESETS = "lae:userEnginePresets:v1";
const LS_MASTER_PRESETS = "lae:userMasterPresets:v1";

function loadJson(key, fallback) {
  try {
    const raw = localStorage.getItem(key);
    if (!raw) return fallback;
    const parsed = JSON.parse(raw);
    return parsed ?? fallback;
  } catch {
    return fallback;
  }
}
function saveJson(key, value) {
  localStorage.setItem(key, JSON.stringify(value));
}
function slugifyId(name) {
  const s = String(name || "")
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/(^-|-$)/g, "");
  return s || `preset-${Date.now()}`;
}

function getUserEnginePresets() {
  const data = loadJson(LS_ENGINE_PRESETS, {});
  return data && typeof data === "object" ? data : {};
}
function userEnginePresetsForType(type) {
  const all = getUserEnginePresets();
  const group = all[type];
  return group && typeof group === "object" ? group : {};
}
function setUserEnginePreset(type, id, preset) {
  const all = getUserEnginePresets();
  if (!all[type] || typeof all[type] !== "object") all[type] = {};
  all[type][id] = preset;
  saveJson(LS_ENGINE_PRESETS, all);
}
function deleteUserEnginePreset(type, id) {
  const all = getUserEnginePresets();
  if (!all[type] || typeof all[type] !== "object") return;
  delete all[type][id];
  saveJson(LS_ENGINE_PRESETS, all);
}

function getUserMasterPresets() {
  const data = loadJson(LS_MASTER_PRESETS, {});
  return data && typeof data === "object" ? data : {};
}
function setUserMasterPreset(id, preset) {
  const all = getUserMasterPresets();
  all[id] = preset;
  saveJson(LS_MASTER_PRESETS, all);
}
function deleteUserMasterPreset(id) {
  const all = getUserMasterPresets();
  delete all[id];
  saveJson(LS_MASTER_PRESETS, all);
}

function isUserPresetKey(key) {
  return String(key || "").startsWith("user:");
}

function updateMasterPresetButtons() {
  const id = String(els.masterPreset?.value || "");
  if (els.deleteMasterPresetBtn) els.deleteMasterPresetBtn.disabled = !isUserPresetKey(id);
}

function refreshMasterPresetSelect({ keepValue = true } = {}) {
  if (!els.masterPreset) return;
  const prev = keepValue ? String(els.masterPreset.value || "") : "";

  els.masterPreset.replaceChildren();

  const none = document.createElement("option");
  none.value = "";
  none.textContent = "Master presets…";
  els.masterPreset.appendChild(none);

  const builtGroup = document.createElement("optgroup");
  builtGroup.label = "Built-in";
  for (const p of MASTER_PRESETS) {
    const opt = document.createElement("option");
    opt.value = p.id;
    opt.textContent = p.name;
    opt.title = p.desc || "";
    builtGroup.appendChild(opt);
  }
  els.masterPreset.appendChild(builtGroup);

  const userPresets = getUserMasterPresets();
  const userKeys = Object.keys(userPresets || {}).sort((a, b) => a.localeCompare(b));
  if (userKeys.length) {
    const userGroup = document.createElement("optgroup");
    userGroup.label = "Yours";
    for (const id of userKeys) {
      const p = userPresets[id];
      const opt = document.createElement("option");
      opt.value = `user:${id}`;
      opt.textContent = String(p?.name || id);
      opt.title = String(p?.desc || "");
      userGroup.appendChild(opt);
    }
    els.masterPreset.appendChild(userGroup);
  }

  const exists =
    prev &&
    (prev.startsWith("user:") ? Boolean(userPresets?.[prev.slice("user:".length)]) : MASTER_PRESETS.some((p) => p.id === prev));
  els.masterPreset.value = exists ? prev : "";
  updateMasterPresetButtons();
}

async function loadTuningManifest() {
  try {
    const res = await fetch(TUNING_MANIFEST_URL);
    if (!res.ok) return { samples: [] };
    const json = await res.json();
    const samples = Array.isArray(json?.samples) ? json.samples.filter((s) => typeof s === "string") : [];
    return { samples };
  } catch {
    return { samples: [] };
  }
}

async function loadTapeSfxManifest() {
  try {
    const res = await fetch(TAPE_SFX_MANIFEST_URL);
    if (!res.ok) return { banks: [] };
    const json = await res.json();
    if (!json || !Array.isArray(json.banks)) return { banks: [] };
    const banks = json.banks
      .map((b) => ({
        id: typeof b.id === "string" ? b.id : "",
        name: typeof b.name === "string" ? b.name : "",
        bed: Array.isArray(b.bed) ? b.bed.filter((x) => typeof x === "string") : [],
        start: Array.isArray(b.start) ? b.start.filter((x) => typeof x === "string") : [],
        end: Array.isArray(b.end) ? b.end.filter((x) => typeof x === "string") : [],
      }))
      .filter((b) => b.id);
    return { banks };
  } catch {
    return { banks: [] };
  }
}

async function loadCamcorderSfxManifest() {
  try {
    const res = await fetch(CAMCORDER_SFX_MANIFEST_URL);
    if (!res.ok) return { camBed: [], windBed: [], windHits: [] };
    const json = await res.json();
    const camBed = Array.isArray(json?.camBed) ? json.camBed.filter((x) => typeof x === "string") : [];
    const windBed = Array.isArray(json?.windBed) ? json.windBed.filter((x) => typeof x === "string") : [];
    const windHits = Array.isArray(json?.windHits) ? json.windHits.filter((x) => typeof x === "string") : [];
    return { camBed, windBed, windHits };
  } catch {
    return { camBed: [], windBed: [], windHits: [] };
  }
}

async function loadTvSfxManifest() {
  try {
    const res = await fetch(TV_SFX_MANIFEST_URL);
    if (!res.ok) return { beds: [] };
    const json = await res.json();
    const beds = Array.isArray(json?.beds) ? json.beds.filter((x) => typeof x === "string") : [];
    return { beds };
  } catch {
    return { beds: [] };
  }
}

class XorShift32 {
  constructor(seed) {
    this.state = (seed >>> 0) || 0x12345678;
  }
  nextU32() {
    let x = this.state >>> 0;
    x ^= x << 13;
    x ^= x >>> 17;
    x ^= x << 5;
    this.state = x >>> 0;
    return this.state;
  }
  nextFloat() {
    return (this.nextU32() >>> 0) / 0xffffffff;
  }
}

function pickOffset(seed, buf) {
  if (!buf) return 0;
  const span = Math.max(0.001, buf.duration - 0.02);
  const off = (seed >>> 0) % 997;
  return (off / 997) * span;
}

function computeGustTimes(duration, seed, hitRate01) {
  const rate = Math.max(0, Math.min(1, hitRate01));
  const lambda = 0.12 + rate * 1.35; // events/sec
  const prng = new XorShift32((seed ^ 0x9e3779b9) >>> 0);
  const times = [];
  if (lambda <= 0.0001 || duration <= 0.05) return times;
  let t = 0;
  const maxEvents = Math.min(500, Math.floor(duration * (0.55 + lambda * 2.2)) + 10);
  while (t < duration && times.length < maxEvents) {
    const u = Math.max(1e-6, Math.min(1 - 1e-6, prng.nextFloat()));
    const dt = -Math.log(u) / lambda;
    t += dt;
    if (t < duration) times.push(t);
  }
  return times;
}

async function loadTuningSample(sourceValue) {
  if (!sourceValue || sourceValue === "synth" || !sourceValue.startsWith("file:")) return null;
  const file = sourceValue.slice("file:".length);
  const urlObj = new URL(`../../audio/${encodeURIComponent(file)}`, import.meta.url);
  const url = urlObj.href;
  if (tuningSampleCache.has(url)) return tuningSampleCache.get(url);

  const res = await fetch(url);
  if (!res.ok) throw new Error(`Failed to fetch tuning sample: ${file}`);
  const ab = await res.arrayBuffer();
  const decodeCtx = new (window.AudioContext || window.webkitAudioContext)();
  try {
    const decoded = await decodeCtx.decodeAudioData(ab.slice(0));
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
  } finally {
    try {
      await decodeCtx.close();
    } catch {
      // ignore
    }
  }
}

function tapeBankById(id) {
  return (tapeSfxManifest.banks || []).find((b) => b.id === id) || null;
}

function tapeDurationOf(buffers) {
  let d = 0;
  for (const b of buffers) d += b?.duration ?? 0;
  return d;
}

function tapePickBedOffset(seed, bedBuffer) {
  if (!bedBuffer) return 0;
  const span = Math.max(0.001, bedBuffer.duration - 0.02);
  const off = (seed >>> 0) % 997;
  return (off / 997) * span;
}

function tapeScheduleSequence(ctx, connectNode, buffers, startTime) {
  const created = [];
  let t = startTime;
  for (const b of buffers) {
    if (!b) continue;
    const src = new AudioBufferSourceNode(ctx, { buffer: b });
    src.connect(connectNode);
    src.start(t, 0);
    created.push(src);
    t += b.duration;
  }
  return { sources: created, duration: t - startTime };
}

async function decodeUrlToMonoAudioBuffer(url) {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`Failed to fetch ${url}`);
  const ab = await res.arrayBuffer();
  const decodeCtx = new (window.AudioContext || window.webkitAudioContext)();
  try {
    const decoded = await decodeCtx.decodeAudioData(ab.slice(0));
    const mono = new AudioBuffer({ length: decoded.length, sampleRate: decoded.sampleRate, numberOfChannels: 1 });
    const out = mono.getChannelData(0);
    if (decoded.numberOfChannels === 1) out.set(decoded.getChannelData(0));
    else {
      const a = decoded.getChannelData(0);
      const b = decoded.getChannelData(1);
      for (let i = 0; i < out.length; i++) out[i] = 0.5 * (a[i] + b[i]);
    }
    return mono;
  } finally {
    try {
      await decodeCtx.close();
    } catch {
      // ignore
    }
  }
}

async function ensureCamcorderSfxDecoded({ camBedSource = "", windBedSource = "", windHitSource = "" } = {}) {
  const needed = [camBedSource, windBedSource, windHitSource].filter((n) => n && !camcorderSfxCache.has(n));
  if (needed.length === 0) return;
  for (const name of needed) {
    const urlObj = new URL(`../../camcorder-engine/audio/${encodeURIComponent(name)}`, import.meta.url);
    const buf = await decodeUrlToMonoAudioBuffer(urlObj.href);
    camcorderSfxCache.set(name, buf);
  }
}

function scheduleCamcorderSfx(ctx, windNode, settings, seed, { startTime = 0, durationSeconds = 0, stopAt = null, looping = false } = {}) {
  const created = [];
  if (!windNode) return created;
  const s = settings || {};

  const windEnabled = Boolean(s.wind);
  const windLevel = Number(s.windLevel ?? 0);
  if (!windEnabled || windLevel <= 0.0001) return created;

  const camLevel = Math.max(0, Math.min(1, Number(s.camLevel ?? 0.35)));
  const windBedLevel = Math.max(0, Math.min(1, Number(s.windBedLevel ?? 0.85)));
  const windHitLevel = Math.max(0, Math.min(1, Number(s.windHitLevel ?? 0.65)));
  const windHitRate = Math.max(0, Math.min(1, Number(s.windHitRate ?? 0.35)));

  const camName = String(s.camBedSource || "");
  const bedName = String(s.windBedSource || "");
  const hitName = String(s.windHitSource || "");

  const camBuf = camName ? camcorderSfxCache.get(camName) || null : null;
  const bedBuf = bedName ? camcorderSfxCache.get(bedName) || null : null;
  const hitBuf = hitName ? camcorderSfxCache.get(hitName) || null : null;

  if (camBuf && camLevel > 0.0001) {
    const g = new GainNode(ctx, { gain: camLevel });
    g.connect(windNode);
    const bed = new AudioBufferSourceNode(ctx, { buffer: camBuf });
    bed.loop = true;
    bed.loopStart = 0;
    bed.loopEnd = camBuf.duration;
    bed.connect(g);
    bed.start(startTime, pickOffset(seed ^ 0x0ddc0ffe, camBuf));
    if (!looping && stopAt != null) bed.stop(stopAt);
    created.push(bed);
  }

  if (bedBuf && windBedLevel > 0.0001) {
    const g = new GainNode(ctx, { gain: windBedLevel });
    g.connect(windNode);
    const bed = new AudioBufferSourceNode(ctx, { buffer: bedBuf });
    bed.loop = true;
    bed.loopStart = 0;
    bed.loopEnd = bedBuf.duration;
    bed.connect(g);
    bed.start(startTime, pickOffset(seed ^ 0x31415927, bedBuf));
    if (!looping && stopAt != null) bed.stop(stopAt);
    created.push(bed);
  }

  if (hitBuf && windHitLevel > 0.0001 && durationSeconds > 0.05) {
    const g = new GainNode(ctx, { gain: windHitLevel });
    g.connect(windNode);
    const times = computeGustTimes(durationSeconds, seed ^ 0x27182818, windHitRate);
    const prng = new XorShift32((seed ^ 0x85ebca6b) >>> 0);
    for (const t of times) {
      const src = new AudioBufferSourceNode(ctx, { buffer: hitBuf });
      src.connect(g);
      const maxOff = Math.max(0, Math.min(0.08, hitBuf.duration - 0.02));
      const off = maxOff > 0 ? prng.nextFloat() * maxOff : 0;
      src.start(startTime + t, off);
      created.push(src);
    }
  }

  return created;
}

async function ensureTapeBankDecoded(bankId) {
  const bank = tapeBankById(bankId);
  if (!bank) return null;
  const files = new Set();
  for (const list of [bank.bed, bank.start, bank.end]) {
    if (!Array.isArray(list)) continue;
    for (const f of list) if (typeof f === "string" && f) files.add(f);
  }
  const needed = Array.from(files).filter((f) => !tapeSfxCache.has(f));
  if (needed.length === 0) return bank;

  for (const name of needed) {
    const urlObj = new URL(`../../tape-engine/audio/${encodeURIComponent(name)}`, import.meta.url);
    const buf = await decodeUrlToMonoAudioBuffer(urlObj.href);
    tapeSfxCache.set(name, buf);
  }
  return bank;
}

function getTapeAssets(bankId) {
  const bank = tapeBankById(bankId);
  if (!bank) return null;
  const bedName = Array.isArray(bank.bed) ? bank.bed[0] : "";
  const bed = bedName ? tapeSfxCache.get(bedName) : null;
  const start = Array.isArray(bank.start) ? bank.start.map((n) => tapeSfxCache.get(n)).filter(Boolean) : [];
  const end = Array.isArray(bank.end) ? bank.end.map((n) => tapeSfxCache.get(n)).filter(Boolean) : [];
  return { bed, start, end };
}

async function ensureTvBedDecoded(name) {
  const file = String(name || "");
  if (!file) return null;
  if (tvSfxCache.has(file)) return tvSfxCache.get(file) || null;
  const urlObj = new URL(`../../television-engine/audio/${encodeURIComponent(file)}`, import.meta.url);
  const buf = await decodeUrlToMonoAudioBuffer(urlObj.href);
  tvSfxCache.set(file, buf);
  return buf;
}

function setState(text) {
  els.state.textContent = text;
}

function readMasterSettings() {
  return {
    masterGain: parseFloat(els.masterGain.value),
    masterHpHz: parseFloat(els.masterHpHz?.value ?? "20"),
    masterLpHz: parseFloat(els.masterLpHz?.value ?? "20000"),
    masterComp: parseFloat(els.masterComp?.value ?? "0"),
    ceiling: parseFloat(els.ceiling.value),
    limiter: parseFloat(els.limiter.value),
    softClip: Boolean(els.softClip.checked),
  };
}

function refreshMasterLabels() {
  const s = readMasterSettings();
  els.masterGainVal.textContent = `${s.masterGain.toFixed(2)}x`;
  if (els.masterHpHzVal) els.masterHpHzVal.textContent = fmtHz(s.masterHpHz);
  if (els.masterLpHzVal) els.masterLpHzVal.textContent = fmtHz(s.masterLpHz);
  if (els.masterCompVal) els.masterCompVal.textContent = pct01(s.masterComp);
  els.ceilingVal.textContent = `${Math.round(s.ceiling * 100)}%`;
  els.limiterVal.textContent = pct01(s.limiter);
  els.optsVal.textContent = `${els.softClip.checked ? "SoftClip" : "NoClip"} | ${els.monoOut.checked ? "Mono" : "Stereo"}`;
}

let sourceBuffer = null;
let audioBuffer = null;
let audioDataSeed = 0;
let tuningEdges = null;

const realtime = {
  ctx: null,
  graph: null,
  src: null,
  playing: false,
  stereo: false,
  extraSources: [],
  endTimers: [],
  tvBedSources: [],
  playWindow: null, // { baseTime, audioStartAt, tapeEndStopAt }
};
let graphStale = true;

const automationRt = {
  ctx: null,
  stereo: false,
  sampleRate: 0,
  layers: [], // [{ graph, gainNode, enabledNode }]
  sum: null,
  master: null,
  sources: [],
  playing: false,
  stopAt: 0,
};

let viewMode = "suite"; // "suite" | module.type
let viewSnapshot = null; // Map(instanceId -> { enabled, wet })

function makeDefaultModules() {
  let nextInstanceId = 1;
  const defaultCartridgePrimary = { quality: 0.55, codec: 0.25, grit: 0.25, noise: 0.15 };
  const defaultCartridgeMacro = computeCartridgeMacroTargets(defaultCartridgePrimary);
  const defaultCdPrimary = { clarity: 0.65, damage: 0.25, tracking: 0.22, jitter: 0.18 };
  const defaultCdMacro = computeCdMacroTargets(defaultCdPrimary);
  return [
    {
      instanceId: nextInstanceId++,
      type: "occlusion",
      name: "Obfuscation",
      desc: "Occlusion: next room / behind a wall",
      enabled: false,
      wet: 1,
      presetKey: "",
      params: {
        distance: 0.35,
        wall: 0.45,
        material: "drywall",
        sourceRoom: 0.35,
        listenerRoom: 0.45,

        hpHz: 50,
        lpHz: 5200,
        leak: 0.08,
        roomMix: 0.22,
        predelayMs: 12,
        outGain: 1,
      },
    },
    {
      instanceId: nextInstanceId++,
      type: "transmission",
      name: "Transmission",
      desc: "AM/Walkie: EQ + drive + dropouts + noise + tuning",
      enabled: true,
      wet: 1,
      presetKey: "",
      params: {
        bandwidth: 0.45,
        drive: 0.35,
        badConnection: 0.25,
        noiseProfile: 0.2,
        pinkNoise: false,
        walkieMode: false,
        walkieFx: "click",
        // Advanced / direct controls (match standalone defaults).
        hpHz: 380,
        lpHz: 5200,
        midGainDb: 0,
        midFreq: 1550,
        midQ: 1.2,
        boxDipDb: 0,
        comp: 0.25,
        asym: 0.1,
        preDrive: 0.25,
        postDrive: 0.35,
        crush: 0,
        wowDepth: 0.25,
        dropRate: 0.25,
        dropDepth: 0.35,
        crackle: 0.25,
        lfoRate: 0.7,
        noiseColor: 0,
        hiss: 0.2,
        walkieThresholdDb: -45,
        walkieMinSilenceMs: 220,
        walkieClickMs: 12,
        walkieClickLevel: 0.65,
        tuningEnable: false,
        tuningMode: "edges",
        tuningSource: "synth",
        tuningAmount: 0.35,
        tuningSnippetMs: 140,
        tuningCutDepth: 0.55,
        outGain: 0.92,
        passes: 1,
      },
    },
    {
      instanceId: nextInstanceId++,
      type: "comms",
      name: "Comms",
      desc: "Telephone/Intercom/PA: narrowband + codec + echo/room",
      enabled: false,
      wet: 1,
      presetKey: "",
      params: {
        mode: "landline",
        bandwidth: 0.4,
        drive: 0.35,
        glitch: 0.2,
        noise: 0.18,
        alarmTone: false,

        // Advanced / direct controls (match standalone defaults).
        hpHz: 280,
        lpHz: 3400,
        midHumpDb: 3.5,
        midFreq: 1850,
        comp: 0.45,
        bits: 12,
        rate: 24000,
        packet: 0.2,
        packetMs: 28,
        hum: 0.25,
        hiss: 0.22,
        toneMix: 0.35,
        ceiling: 0.92,
        outGain: 0.95,

        echoMix: 0,
        echoMs: 180,
        echoFb: 0.28,
        echoTone: 0.55,

        verbMix: 0,
        verbMs: 240,
        verbDamp: 0.45,
      },
    },
    {
      instanceId: nextInstanceId++,
      type: "tape",
      name: "Tape",
      desc: "Cassette/VHS audio: wow/flutter + hiss + dropouts",
      enabled: false,
      wet: 1,
      presetKey: "",
      params: {
        quality: 0.55,
        age: 0.35,
        wow: 0.25,
        glitch: 0.18,
        sfxEnable: false,

        // Advanced / direct controls (match standalone defaults).
        hpHz: 35,
        lpHz: 11000,
        headBumpDb: 2.2,
        headBumpHz: 85,
        drive: 0.35,
        comp: 0.28,
        speed: 1,
        wowDepthMs: 3.5,
        flutterDepthMs: 1.2,
        hiss: 0.12,
        hum: 0.05,
        dropout: 0.18,
        dropoutMs: 38,
        ceiling: 0.92,
        outGain: 0.98,

        sfxSource: "",
        sfxLevel: 0.22,
        sfxMode: "bed",
      },
    },
    {
      instanceId: nextInstanceId++,
      type: "television",
      name: "Television",
      desc: "CRT/TV: speaker EQ + AGC + hum + static + whine",
      enabled: false,
      wet: 1,
      presetKey: "",
      params: {
        vibe: 0.45,
        speaker: 0.55,
        agc: 0.22,
        static: 0.12,
        hum: 0.18,
        whine: 0.08,

        hpHz: 70,
        lpHz: 9000,
        midHumpDb: 1.2,
        midFreq: 1800,
        noiseHiss: 0.55,
        noiseCrackle: 0.08,

        bedEnable: false,
        bedLevel: 0.22,
        bedSource: "",

        outGain: 1,
      },
    },
    {
      instanceId: nextInstanceId++,
      type: "cartridge",
      name: "Cartridge",
      desc: "Retro hardware: bitcrush/codec + grit + micro delay/verb",
      enabled: false,
      wet: 1,
      presetKey: "",
      params: {
        quality: 0.55,
        codec: 0.25,
        grit: 0.25,
        noise: 0.15,

        // Macro-derived defaults (standalone applies macro targets on load).
        bits: defaultCartridgeMacro.bits,
        rate: defaultCartridgeMacro.rate,
        lpHz: defaultCartridgeMacro.lpHz,
        jitter: defaultCartridgeMacro.jitter,
        preEmph: defaultCartridgeMacro.preEmph,
        mulaw: defaultCartridgeMacro.mulaw,
        blockMs: defaultCartridgeMacro.blockMs,
        sat: defaultCartridgeMacro.sat,
        hum: defaultCartridgeMacro.hum,
        whine: defaultCartridgeMacro.whine,
        outGain: defaultCartridgeMacro.outGain,

        // Direct controls (match standalone defaults).
        bleepsEnable: false,
        bleepsMix: 0.18,
        bleepsRate: 3,
        bleepsWave: "random",
        bleepsVibrato: 0.35,
        bleepsPitch: 0.55,

        microDelayMs: 8,
        microDelayMix: 0.18,
        verb: 0.22,
        verbMs: 45,
        wet: 1,
        ceiling: 0.92,
        limiter: 0.35,
        edge: 0.25,
        noiseTrack: 0.6,
        dcDrift: 0.15,
        hpHz: 70,
        speaker: 0.45,

        dither: true,
        noiseShaping: false,
      },
    },
    {
      instanceId: nextInstanceId++,
      type: "cd",
      name: "CD",
      desc: "Optical errors: repeats/holds + scratches + jitter + car comp",
      enabled: false,
      wet: 1,
      presetKey: "",
      params: {
        ...defaultCdPrimary,
        carComp: defaultCdMacro.carComp ?? 0,
        softClip: true,
        mode: defaultCdMacro.mode ?? "hold",
        errorRate: defaultCdMacro.errorRate ?? 0.18,
        burstMs: defaultCdMacro.burstMs ?? 24,
        repeatMs: defaultCdMacro.repeatMs ?? 42,
        scratchRate: defaultCdMacro.scratchRate ?? 0.25,
        scratchAmt: defaultCdMacro.scratchAmt ?? 0.35,
        jitterMs: defaultCdMacro.jitterMs ?? 0.18,
        jitterRate: defaultCdMacro.jitterRate ?? 38,
        hfLoss: defaultCdMacro.hfLoss ?? 0.1,
        servoNoise: defaultCdMacro.servoNoise ?? 0.12,
        ceiling: defaultCdMacro.ceiling ?? 0.94,
        outGain: defaultCdMacro.outGain ?? 0.98,
      },
    },
    {
      instanceId: nextInstanceId++,
      type: "camcorder",
      name: "Camcorder",
      desc: "Muffled mic + AGC + movement + corruption + wind",
      enabled: false,
      wet: 1,
      presetKey: "",
      params: {
        coverage: 0.35,
        movement: 0.25,
        corruption: 0.18,
        agc: 0.35,
        wind: false,
        windLevel: 0.95,
        camLevel: 0.35,
        windBedLevel: 0.85,
        windHitLevel: 0.65,
        windHitRate: 0.35,
        camBedSource: "",
        windBedSource: "",
        windHitSource: "",
        hpHz: 55,
        lpHz: 9200,
        boxDb: 3.2,
        boxHz: 1650,
        agcAmt: 0.55,
        agcSpeed: 0.45,
        clip: 0.25,
        crush: 0.12,
        bits: 12,
        rate: 24000,
        drop: 0.18,
        dropMs: 28,
        dropMode: "hold",
        repeatMs: 48,
        chirp: 0.15,
        handling: 0.22,
        rub: 0.18,
        hiss: 0.12,
        ceiling: 0.92,
        outGain: 0.98,
      },
    },
    {
      instanceId: nextInstanceId++,
      type: "conference",
      name: "Conference",
      desc: "VoIP/call: packet loss + jitter + codec + robot",
      enabled: false,
      wet: 1,
      presetKey: "",
      params: {
        mode: "discord",
        bandwidth: 0.45,
        codec: 0.35,
        dropouts: 0.25,
        jitter: 0.2,
        robot: 0.12,
        noise: 0.12,

        // Advanced / direct controls (match Conference Engine defaults).
        hpHz: 260,
        lpHz: 4200,
        midHumpDb: 2.2,
        midFreq: 1750,
        concealMode: "hold",
        packetLoss: 0.18,
        packetMs: 24,
        repeatMs: 42,
        jitterMs: 0.12,
        jitterRate: 34,
        gate: 0.12,
        bits: 12,
        rate: 24000,
        ceiling: 0.92,
        outGain: 0.98,
      },
    },
  ];
}

let modules = makeDefaultModules();

const MAX_LAYERS = 4;
const automation = {
  bpm: 120,
  pxPerSec: 100,
  snap: true,
  activeLayer: 0,
  layers: [],
  target: "",
  ui: {
    trackByIndex: new Map(),
    laneDragging: null,
  },
};

function makeAutomationLayer(index) {
  return {
    index,
    name: `Layer ${index + 1}`,
    fileName: "Empty",
    buffer: null,
    peaks: null,
    seed: (0x1234abcd ^ Math.imul(index + 1, 0x9e3779b9)) >>> 0,
    tuningEdges: null,
    clipStartSec: 0,
    gain: 1,
    enabled: true,
    modules: makeDefaultModules().map((m) => ({ ...m, params: { ...(m.params || {}) } })),
    automations: {
      // Populated lazily per-module when needed.
      modules: new Map(), // instanceId -> { wet: Lane, enabled: Lane }
      layer: new Map(), // key -> Lane
      master: new Map(), // key -> Lane
    },
  };
}

function getCurrentModules() {
  if (viewMode === "automation") {
    const layer = automation.layers[automation.activeLayer] || null;
    return layer?.modules || [];
  }
  return modules;
}

function getVisibleModules() {
  const current = getCurrentModules();
  if (viewMode === "suite" || viewMode === "automation") return current;
  return current.filter((m) => m.type === viewMode);
}

function syncModuleRowColumns() {
  const count = Math.max(1, getVisibleModules().length);
  els.moduleRow?.style?.setProperty("--cols", String(count));
}

class Lane {
  constructor(kind = "float") {
    this.kind = kind === "bool" ? "bool" : "float";
    this.points = [];
  }

  isEmpty() {
    return this.points.length === 0;
  }

  addPoint(t, v) {
    const time = Math.max(0, Number(t) || 0);
    const value = this.kind === "bool" ? (Number(v) >= 0.5 ? 1 : 0) : clamp01(Number(v) || 0);
    this.points.push({ t: time, v: value });
    this.points.sort((a, b) => a.t - b.t);
  }

  removeNearest(t, epsSec = 0.08) {
    if (!this.points.length) return false;
    let best = -1;
    let bestDt = Infinity;
    for (let i = 0; i < this.points.length; i++) {
      const dt = Math.abs(this.points[i].t - t);
      if (dt < bestDt) {
        bestDt = dt;
        best = i;
      }
    }
    if (best >= 0 && bestDt <= epsSec) {
      this.points.splice(best, 1);
      return true;
    }
    return false;
  }

  valueAt(t, fallback) {
    const time = Math.max(0, Number(t) || 0);
    if (!this.points.length) return fallback;

    if (this.kind === "bool") {
      let v = this.points[0].v;
      for (const p of this.points) {
        if (p.t <= time) v = p.v;
        else break;
      }
      return v;
    }

    if (time <= this.points[0].t) return this.points[0].v;
    const last = this.points[this.points.length - 1];
    if (time >= last.t) return last.v;
    for (let i = 0; i < this.points.length - 1; i++) {
      const a = this.points[i];
      const b = this.points[i + 1];
      if (a.t <= time && time <= b.t) {
        const dt = b.t - a.t || 1e-6;
        const u = (time - a.t) / dt;
        return a.v + (b.v - a.v) * u;
      }
    }
    return last.v;
  }
}

async function applyViewMode(nextMode) {
  const mode = String(nextMode || "suite");
  const prevMode = viewMode;
  if (mode === prevMode) return;
  if (prevMode === "automation" && mode !== "automation") await teardownAutomationRealtime();
  viewMode = mode;

  const isAutomation = viewMode === "automation";
  if (els.automationView) els.automationView.classList.toggle("hidden", !isAutomation);

  if (isAutomation) {
    graphStale = true;
    await teardownRealtime();
    syncModuleRowColumns();
    renderModules();
    renderAutomationUi();
    setState(automation.layers.some((l) => l?.buffer) ? "Ready" : "Idle");
    return;
  }

  if (viewMode === "suite") {
    if (viewSnapshot) {
      const snap = viewSnapshot;
      for (const m of modules) {
        const s = snap.get(m.instanceId);
        if (!s) continue;
        m.enabled = Boolean(s.enabled);
        m.wet = typeof s.wet === "number" ? clamp01(s.wet) : m.wet;
      }
      viewSnapshot = null;
    }
  } else {
    if (!viewSnapshot) viewSnapshot = new Map(modules.map((m) => [m.instanceId, { enabled: m.enabled, wet: m.wet }]));
    for (const m of modules) {
      if (m.type === viewMode) {
        m.enabled = true;
        m.wet = 1;
      } else {
        m.enabled = false;
      }
    }
  }

  graphStale = true;
  await teardownRealtime();
  syncModuleRowColumns();
  renderModules();
  setState(audioBuffer ? "Ready" : "Idle");
  if (els.playBtn) els.playBtn.disabled = !audioBuffer;
  if (els.exportBtn) els.exportBtn.disabled = !audioBuffer;
}

function computeTapeMacroTargets(primary) {
  const quality = clamp01(primary.quality ?? 0.55);
  const age = clamp01(primary.age ?? 0.35);
  const wow = clamp01(primary.wow ?? 0.25);
  const glitch = clamp01(primary.glitch ?? 0.18);

  const q = Math.pow(1 - quality, 1.4);
  const a = Math.pow(age, 1.25);
  const w = Math.pow(wow, 1.3);
  const g = Math.pow(glitch, 1.35);

  const lpHz = Math.round(17500 - q * 14500);
  const hpHz = Math.round(25 + q * 90);

  const hiss = clamp01(0.03 + q * 0.22);
  const hum = clamp01(0.01 + q * 0.06);

  const drive = clamp01(0.08 + a * 0.85);
  const comp = clamp01(0.12 + a * 0.5);

  const headBumpDb = Math.round((1.4 + a * 5.8) * 20) / 20;
  const headBumpHz = Math.round(70 + a * 45);

  const wowDepthMs = Math.round((1.2 + w * 12.5) * 10) / 10;
  const flutterDepthMs = Math.round((0.4 + w * 4.8) * 10) / 10;

  const dropout = clamp01(g * (0.7 + 0.35 * a));
  const dropoutMs = Math.round(18 + g * 140);

  const speed = Math.round((1 - w * 0.05) * 1000) / 1000;

  const ceiling = 0.92 - a * 0.06;
  const outGain = Math.round((0.96 + a * 0.18) * 100) / 100;

  return { lpHz, hpHz, hiss, hum, drive, comp, headBumpDb, headBumpHz, wowDepthMs, flutterDepthMs, dropout, dropoutMs, speed, ceiling, outGain };
}

function computeTelevisionMacroTargets(primary) {
  const vibe = clamp01(primary.vibe ?? 0.45);
  const speaker = clamp01(primary.speaker ?? 0.55);
  const agc = clamp01(primary.agc ?? 0.22);
  const staticAmt = clamp01(primary.static ?? 0.12);
  const hum = clamp01(primary.hum ?? 0.18);
  const whine = clamp01(primary.whine ?? 0.08);

  const v = Math.pow(vibe, 1.15);
  const sp = Math.pow(speaker, 1.15);
  const a = Math.pow(agc, 1.25);
  const st = Math.pow(staticAmt, 1.2);

  const hpHz = Math.round(45 + (1 - sp) * 110 + v * 30);
  const lpHz = Math.round(16000 - (1 - sp) * 10000 - v * 2600);
  const midHumpDb = Math.round((0.6 + (1 - sp) * 2.4 + v * 0.6) * 20) / 20;
  const midFreq = Math.round(1550 + (1 - sp) * 650);

  const noiseHiss = clamp01(0.45 + st * 0.5);
  const noiseCrackle = clamp01(0.04 + v * 0.12);

  const compAmt = clamp01(0.08 + a * 0.65);
  const drive = 0.35 + a * 1.2;

  return {
    hpHz,
    lpHz,
    midHumpDb,
    midFreq,
    noiseHiss,
    noiseCrackle,
    compAmt,
    drive,
    hum,
    whine,
  };
}

function computeOcclusionMacroTargets(primary) {
  const distance = clamp01(primary.distance ?? 0.35);
  const wall = clamp01(primary.wall ?? 0.45);
  const sourceRoom = clamp01(primary.sourceRoom ?? 0.35);
  const listenerRoom = clamp01(primary.listenerRoom ?? 0.45);
  const materialRaw = String(primary.material ?? "drywall");
  const material =
    materialRaw === "brick" || materialRaw === "wood" || materialRaw === "curtain" || materialRaw === "door" || materialRaw === "glass"
      ? materialRaw
      : "drywall";

  const mat =
    material === "brick"
      ? { lpMin: 1100, damp: 0.75, leakBias: -0.05 }
      : material === "wood"
        ? { lpMin: 1500, damp: 0.65, leakBias: 0.02 }
        : material === "curtain"
          ? { lpMin: 2200, damp: 0.55, leakBias: 0.18 }
          : material === "door"
            ? { lpMin: 1350, damp: 0.72, leakBias: 0.08 }
            : material === "glass"
              ? { lpMin: 2600, damp: 0.45, leakBias: 0.25 }
              : { lpMin: 1800, damp: 0.68, leakBias: 0.05 };

  const d = Math.pow(distance, 1.15);
  const w = Math.pow(wall, 1.2);
  const room = clamp01(0.45 * sourceRoom + 0.55 * listenerRoom);

  const hpHz = Math.round(35 + d * 75 + w * 45);
  const lpHz = Math.round(16000 - (d * 5500 + w * 9500));
  const lp = Math.max(mat.lpMin + w * 250, lpHz);

  const leak = clamp01(0.03 + (1 - w) * 0.18 + mat.leakBias);
  const roomMix = clamp01(0.08 + room * 0.32 + d * 0.18);
  const predelayMs = Math.round(6 + room * 26 + d * 10);
  const damp = clamp01(mat.damp + room * 0.12);
  const roomSize = clamp01(room);
  const outGain = Math.round((1.0 - d * 0.18) * 100) / 100;

  return { material, hpHz, lpHz: lp, leak, roomMix, predelayMs, damp, roomSize, outGain };
}

function computeCartridgeMacroTargets(primary) {
  const quality = clamp01(primary.quality ?? 0.55);
  const codec = clamp01(primary.codec ?? 0.25);
  const grit = clamp01(primary.grit ?? 0.25);
  const noise = clamp01(primary.noise ?? 0.15);

  const qCurve = Math.pow(1 - quality, 1.6);
  const bits = Math.round(14 - qCurve * 10);
  const rate = Math.round(42000 - qCurve * 34000);
  const lpHz = Math.round(16000 - qCurve * 13500);
  const jitter = clamp01(0.02 + qCurve * 0.45);

  const cCurve = Math.pow(codec, 1.15);
  const mulaw = clamp01(cCurve * 0.95);
  const blockMs = Math.round(cCurve * cCurve * 42);
  const preEmph = clamp01(0.08 + cCurve * 0.7);

  const gCurve = Math.pow(grit, 1.25);
  const sat = clamp01(0.12 + gCurve * 0.88);
  const hum = clamp01(gCurve * 0.25);
  const whine = clamp01(0.08 + gCurve * 0.7);
  const outGain = 0.98 - gCurve * 0.18;

  return { bits, rate, lpHz, jitter, mulaw, blockMs, preEmph, sat, hum, whine, noise, outGain };
}

function computeCommsMacroTargets(primary) {
  const mode = primary.mode ?? "landline";
  const bandwidth = clamp01(primary.bandwidth ?? 0.4);
  const drive = clamp01(primary.drive ?? 0.35);
  const glitch = clamp01(primary.glitch ?? 0.2);
  const noise = clamp01(primary.noise ?? 0.18);

  const narrow = Math.pow(1 - bandwidth, 1.35);
  const drv = Math.pow(drive, 1.25);
  const g = Math.pow(glitch, 1.35);
  const n = Math.pow(noise, 1.2);

  const base =
    mode === "cell"
      ? { hp: 220, hpR: 360, lp: 3700, lpR: 1600, hump: 2.0, humpR: 4.2, mid: 1700, midR: 450, comp: 0.58, out: 1.02, ceil: 0.92 }
      : mode === "intercom"
        ? { hp: 340, hpR: 380, lp: 3300, lpR: 1100, hump: 4.2, humpR: 6.0, mid: 1950, midR: 450, comp: 0.66, out: 0.98, ceil: 0.9 }
        : mode === "pa"
          ? { hp: 160, hpR: 280, lp: 6800, lpR: 2600, hump: 1.6, humpR: 3.2, mid: 1500, midR: 450, comp: 0.42, out: 1.15, ceil: 0.88 }
          : mode === "alarm"
            ? { hp: 250, hpR: 360, lp: 7600, lpR: 3400, hump: 1.0, humpR: 3.0, mid: 1600, midR: 700, comp: 0.46, out: 1.05, ceil: 0.9 }
            : { hp: 250, hpR: 320, lp: 4300, lpR: 1900, hump: 2.8, humpR: 5.4, mid: 1850, midR: 380, comp: 0.54, out: 0.95, ceil: 0.92 };

  const hpHz = Math.round(base.hp + narrow * base.hpR);
  const lpHz = Math.round(base.lp - narrow * base.lpR);
  const midHumpDb = Math.round((base.hump + narrow * base.humpR) * 20) / 20;
  const midFreq = Math.round(base.mid + (0.55 - narrow) * base.midR);

  const comp = clamp01(base.comp + drv * 0.38);
  const bitsBase = mode === "pa" ? 14 : 13;
  const bits = Math.round(clamp01(1 - g) * (bitsBase - 4) + 4);
  const rate = Math.round(46000 - g * 38000);

  const packetScale = mode === "cell" ? 0.72 : mode === "alarm" ? 0.35 : 0.25;
  const packet = clamp01(g * packetScale);
  const packetMs = Math.round(10 + g * (mode === "cell" ? 120 : 75));

  const hum = clamp01(0.06 + n * (mode === "intercom" ? 0.55 : 0.4));
  const hiss = clamp01(0.08 + n * 0.55);
  const toneMix = clamp01((mode === "alarm" ? 0.45 : 0.22) + n * 0.15);

  const ceiling = clamp01(base.ceil);
  const outGain = Math.round((base.out + drv * 0.12) * 100) / 100;

  const roomBase = mode === "intercom" ? 0.18 : mode === "pa" ? 0.12 : mode === "alarm" ? 0.14 : 0.05;
  const verbMix = clamp01(roomBase + n * 0.12);
  const verbMs = Math.round((mode === "intercom" ? 420 : mode === "pa" ? 540 : mode === "alarm" ? 360 : 220) * (0.75 + 0.55 * n));
  const verbDamp = clamp01(mode === "intercom" ? 0.7 : mode === "pa" ? 0.55 : 0.45 + n * 0.1);

  const echoBase = mode === "pa" ? 0.14 : mode === "intercom" ? 0.08 : mode === "alarm" ? 0.05 : 0.03;
  const echoMix = clamp01(echoBase + g * 0.08);
  const echoMs = Math.round((mode === "pa" ? 240 : mode === "intercom" ? 260 : 180) * (0.85 + 0.35 * g));
  const echoFb = clamp01(0.12 + (mode === "pa" ? 0.35 : 0.22) * g);
  const echoTone = clamp01(mode === "intercom" ? 0.45 : 0.6 + n * 0.15);

  return { hpHz, lpHz, midHumpDb, midFreq, comp, bits, rate, packet, packetMs, hum, hiss, toneMix, ceiling, outGain, echoMix, echoMs, echoFb, echoTone, verbMix, verbMs, verbDamp };
}

function computeConferenceMacroTargets(primary) {
  const mode = String(primary.mode ?? "discord");
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
    gate,
    bits,
    rate,
    ceiling,
    outGain,
  };
}

function computeCdMacroTargets(primary) {
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

function computeCamcorderMacroTargets(primary) {
  const coverage = clamp01(primary.coverage ?? 0.35);
  const movement = clamp01(primary.movement ?? 0.25);
  const corruption = clamp01(primary.corruption ?? 0.18);
  const agc = clamp01(primary.agc ?? 0.35);

  const cov = Math.pow(coverage, 1.3);
  const mov = Math.pow(movement, 1.25);
  const cor = Math.pow(corruption, 1.3);
  const drv = Math.pow(agc, 1.25);

  const lpHz = Math.round(15000 - cov * 12000);
  const hpHz = Math.round(45 + cov * 70);
  const boxDb = Math.round((2.2 + cov * 6.5) * 20) / 20;
  const boxHz = Math.round(1500 + cov * 250);

  const agcAmt = clamp01(0.45 + drv * 0.45 + cov * 0.15);
  const agcSpeed = clamp01(0.25 + drv * 0.5);
  const clip = clamp01(0.08 + drv * 0.75);

  const crush = clamp01(0.05 + cor * 0.45);
  const bits = Math.round(14 - cor * 6);
  const rate = Math.round(42000 - cor * 26000);

  const drop = clamp01(0.05 + cor * 0.8);
  const dropMs = Math.round(16 + cor * 120);
  const dropMode = cor > 0.6 ? "repeat" : cor > 0.22 ? "hold" : "interp";
  const repeatMs = Math.round(30 + cor * 120);
  const chirp = clamp01(cor * 0.6);

  const handling = clamp01(0.06 + mov * 0.7);
  const rub = clamp01(0.04 + mov * 0.7);
  const hiss = clamp01(0.06 + cov * 0.18 + cor * 0.08);

  const ceiling = 0.94 - drv * 0.1;
  const outGain = Math.round((0.98 + drv * 0.18) * 100) / 100;
  const wind = mov > 0.55 && cov > 0.45;

  const windLevel = wind ? 1.05 : 0.95;
  const camLevel = 0.35 + cov * 0.12;
  const windBedLevel = clamp01(0.65 + mov * 0.25);
  const windHitLevel = clamp01(0.45 + mov * 0.35);
  const windHitRate = clamp01(0.18 + mov * 0.62);

  return {
    hpHz,
    lpHz,
    boxDb,
    boxHz,
    agcAmt,
    agcSpeed,
    clip,
    crush,
    bits,
    rate,
    drop,
    dropMs,
    dropMode,
    repeatMs,
    chirp,
    handling,
    rub,
    hiss,
    ceiling,
    outGain,
    wind,
    windLevel,
    camLevel,
    windBedLevel,
    windHitLevel,
    windHitRate,
  };
}

function settingsForModule(m) {
  const p = m.params || {};
  const has = (k) => Object.prototype.hasOwnProperty.call(p, k);
  const num = (k, fallback) => {
    if (!has(k)) return fallback;
    const v = Number(p[k]);
    return Number.isFinite(v) ? v : fallback;
  };
  const bool = (k, fallback) => (has(k) ? Boolean(p[k]) : fallback);
  const str = (k, fallback) => (has(k) ? String(p[k] ?? "") : fallback);

  if (m.type === "occlusion") {
    const materialRaw = str("material", "drywall");
    const material =
      materialRaw === "brick" || materialRaw === "wood" || materialRaw === "curtain" || materialRaw === "door" || materialRaw === "glass"
        ? materialRaw
        : "drywall";
    return {
      distance: clamp01(num("distance", 0.35)),
      wall: clamp01(num("wall", 0.45)),
      material,
      sourceRoom: clamp01(num("sourceRoom", 0.35)),
      listenerRoom: clamp01(num("listenerRoom", 0.45)),

      hpHz: num("hpHz", 50),
      lpHz: num("lpHz", 5200),
      dipHz: num("dipHz", 1600),
      dipDb: num("dipDb", -2),
      dipQ: num("dipQ", 1.1),
      bumpHz: num("bumpHz", 420),
      bumpDb: num("bumpDb", 1.2),
      bumpQ: num("bumpQ", 0.95),

      leak: clamp01(num("leak", 0.08)),
      roomMix: clamp01(num("roomMix", 0.22)),
      predelayMs: num("predelayMs", 12),
      roomSize: clamp01(num("roomSize", 0.5)),
      damp: clamp01(num("damp", 0.68)),
      outGain: num("outGain", 1),
    };
  }

  if (m.type === "transmission") {
    const bw = clamp01(num("bandwidth", 0.45));
    const drive = clamp01(num("drive", 0.35));
    const bad = clamp01(num("badConnection", 0.25));
    const noiseProfile = clamp01(num("noiseProfile", 0.2));
    const mapped = mapBandwidth(bw);

    const autoColor = Math.max(0, (noiseProfile - 0.55) * 2);
    const pinkNoise = bool("pinkNoise", false);

    return {
      bandwidth: bw,
      drive,
      badConnection: bad,
      noiseProfile,
      pinkNoise,

      hpHz: num("hpHz", mapped.hp),
      lpHz: num("lpHz", mapped.lp),
      midGainDb: num("midGainDb", mapped.midGainDb),
      midFreq: num("midFreq", mapped.midFreq),
      midQ: num("midQ", mapped.midQ),
      boxDipDb: num("boxDipDb", (1 - bw) * 2.2),

      asym: clamp01(num("asym", drive * 0.6)),
      comp: clamp01(num("comp", 0.18 + drive * 0.65)),
      preDrive: clamp01(num("preDrive", Math.pow(drive, 0.85) * 0.75)),
      postDrive: clamp01(num("postDrive", drive)),
      crush: clamp01(num("crush", 0)),

      wowDepth: clamp01(num("wowDepth", bad)),
      dropRate: clamp01(num("dropRate", bad)),
      dropDepth: clamp01(num("dropDepth", bad)),
      crackle: clamp01(num("crackle", bad)),
      lfoRate: num("lfoRate", 0.45 + bad * 1.6),

      noiseColor: clamp01(num("noiseColor", pinkNoise ? 1 : autoColor)),
      hiss: clamp01(num("hiss", noiseProfile * 0.95)),

      walkieMode: bool("walkieMode", false),
      walkieFx: str("walkieFx", "dispatch") === "dispatch" ? "dispatch" : "click",
      walkieThresholdDb: num("walkieThresholdDb", -45),
      walkieMinSilenceMs: num("walkieMinSilenceMs", 220),
      walkieClickMs: num("walkieClickMs", 12),
      walkieClickLevel: clamp01(num("walkieClickLevel", 0.65)),

      tuningEnable: bool("tuningEnable", false),
      tuningMode: str("tuningMode", "edges") === "search" ? "search" : "edges",
      tuningSource: str("tuningSource", "synth") || "synth",
      tuningAmount: clamp01(num("tuningAmount", 0.35)),
      tuningSnippetMs: num("tuningSnippetMs", 140),
      tuningCutDepth: clamp01(num("tuningCutDepth", 0.55)),

      outGain: num("outGain", 0.92),
      passes: Math.round(Math.max(1, Math.min(12, num("passes", 1)))),
    };
  }

  if (m.type === "tape") {
    const t = computeTapeMacroTargets(p);
    return {
      ...t,
      wow: clamp01(num("wow", 0.25)),
      sfxEnable: bool("sfxEnable", false),
      sfxSource: str("sfxSource", "") || "",
      sfxLevel: clamp01(num("sfxLevel", 0.22)),
      sfxMode: str("sfxMode", "bed") || "bed",

      hpHz: num("hpHz", t.hpHz),
      lpHz: num("lpHz", t.lpHz),
      headBumpDb: num("headBumpDb", t.headBumpDb),
      headBumpHz: num("headBumpHz", t.headBumpHz),
      drive: clamp01(num("drive", t.drive)),
      comp: clamp01(num("comp", t.comp)),
      speed: num("speed", t.speed),
      wowDepthMs: num("wowDepthMs", t.wowDepthMs),
      flutterDepthMs: num("flutterDepthMs", t.flutterDepthMs),
      hiss: clamp01(num("hiss", t.hiss)),
      hum: clamp01(num("hum", t.hum)),
      dropout: clamp01(num("dropout", t.dropout)),
      dropoutMs: num("dropoutMs", t.dropoutMs),
      ceiling: num("ceiling", t.ceiling),
      outGain: num("outGain", t.outGain),
    };
  }

  if (m.type === "television") {
    const vibe = clamp01(num("vibe", 0.45));
    const speaker = clamp01(num("speaker", 0.55));
    const agc = clamp01(num("agc", 0.22));
    const staticAmt = clamp01(num("static", 0.12));
    const hum = clamp01(num("hum", 0.18));
    const whine = clamp01(num("whine", 0.08));

    const t = computeTelevisionMacroTargets({ vibe, speaker, agc, static: staticAmt, hum, whine });

    return {
      vibe,
      speaker,
      agc,
      static: staticAmt,
      hum,
      whine,

      hpHz: num("hpHz", t.hpHz),
      lpHz: num("lpHz", t.lpHz),
      midHumpDb: num("midHumpDb", t.midHumpDb),
      midFreq: num("midFreq", t.midFreq),
      noiseHiss: clamp01(num("noiseHiss", t.noiseHiss)),
      noiseCrackle: clamp01(num("noiseCrackle", t.noiseCrackle)),

      bedEnable: bool("bedEnable", false),
      bedLevel: clamp01(num("bedLevel", 0.22)),
      bedSource: str("bedSource", "") || "",

      outGain: num("outGain", 1),
    };
  }

  if (m.type === "cartridge") {
    const t = computeCartridgeMacroTargets(p);
    return {
      ...t,
      dither: bool("dither", true),
      noiseShaping: bool("noiseShaping", false),
      bits: Math.round(Math.max(2, Math.min(16, num("bits", t.bits)))),
      rate: num("rate", t.rate),
      jitter: clamp01(num("jitter", t.jitter)),
      lpHz: num("lpHz", t.lpHz),
      hpHz: num("hpHz", 70),

      preEmph: clamp01(num("preEmph", t.preEmph)),
      mulaw: clamp01(num("mulaw", t.mulaw)),
      blockMs: num("blockMs", t.blockMs),

      sat: clamp01(num("sat", t.sat)),
      edge: clamp01(num("edge", 0.25)),
      noiseTrack: clamp01(num("noiseTrack", 0.6)),
      dcDrift: clamp01(num("dcDrift", 0.15)),
      speaker: clamp01(num("speaker", 0.45)),

      hum: clamp01(num("hum", t.hum)),
      whine: clamp01(num("whine", t.whine)),
      outGain: num("outGain", t.outGain),

      microDelayMs: num("microDelayMs", 8),
      microDelayMix: clamp01(num("microDelayMix", 0.18)),
      verb: clamp01(num("verb", 0.22)),
      verbMs: num("verbMs", 45),

      wet: clamp01(num("wet", 1)),
      ceiling: num("ceiling", 0.92),
      limiter: clamp01(num("limiter", 0.35)),

      bleepsEnable: bool("bleepsEnable", false),
      bleepsMix: clamp01(num("bleepsMix", 0.18)),
      bleepsRate: num("bleepsRate", 3),
      bleepsWave: str("bleepsWave", "random") || "random",
      bleepsVibrato: clamp01(num("bleepsVibrato", 0.35)),
      bleepsPitch: clamp01(num("bleepsPitch", 0.55)),
    };
  }

  if (m.type === "comms") {
    const t = computeCommsMacroTargets(p);
    return {
      ...t,
      mode: str("mode", "landline") || "landline",
      bandwidth: clamp01(num("bandwidth", 0.4)),
      drive: clamp01(num("drive", 0.35)),
      glitch: clamp01(num("glitch", 0.2)),
      noise: clamp01(num("noise", 0.18)),
      alarmTone: bool("alarmTone", false),
      hpHz: num("hpHz", t.hpHz),
      lpHz: num("lpHz", t.lpHz),
      midHumpDb: num("midHumpDb", t.midHumpDb),
      midFreq: num("midFreq", t.midFreq),
      comp: clamp01(num("comp", t.comp)),
      bits: num("bits", t.bits),
      rate: num("rate", t.rate),
      packet: clamp01(num("packet", t.packet)),
      packetMs: num("packetMs", t.packetMs),
      hum: clamp01(num("hum", t.hum)),
      hiss: clamp01(num("hiss", t.hiss)),
      toneMix: clamp01(num("toneMix", t.toneMix)),
      ceiling: num("ceiling", t.ceiling),
      outGain: num("outGain", t.outGain),
      echoMix: clamp01(num("echoMix", t.echoMix)),
      echoMs: num("echoMs", t.echoMs),
      echoFb: clamp01(num("echoFb", t.echoFb)),
      echoTone: clamp01(num("echoTone", t.echoTone)),
      verbMix: clamp01(num("verbMix", t.verbMix)),
      verbMs: num("verbMs", t.verbMs),
      verbDamp: clamp01(num("verbDamp", t.verbDamp)),
    };
  }

  if (m.type === "conference") {
    const t = computeConferenceMacroTargets(p);
    return {
      ...t,
      mode: str("mode", "discord") || "discord",
      bandwidth: clamp01(num("bandwidth", 0.45)),
      codec: clamp01(num("codec", 0.35)),
      dropouts: clamp01(num("dropouts", 0.25)),
      jitter: clamp01(num("jitter", 0.2)),
      robot: clamp01(num("robot", 0.12)),
      noise: clamp01(num("noise", 0.12)),

      hpHz: num("hpHz", t.hpHz),
      lpHz: num("lpHz", t.lpHz),
      midHumpDb: num("midHumpDb", t.midHumpDb),
      midFreq: num("midFreq", t.midFreq),

      concealMode: str("concealMode", t.concealMode) || t.concealMode,
      packetLoss: clamp01(num("packetLoss", t.packetLoss)),
      packetMs: num("packetMs", t.packetMs),
      repeatMs: num("repeatMs", t.repeatMs),
      jitterMs: num("jitterMs", t.jitterMs),
      jitterRate: num("jitterRate", t.jitterRate),
      gate: clamp01(num("gate", t.gate)),
      bits: num("bits", t.bits),
      rate: num("rate", t.rate),
      ceiling: num("ceiling", t.ceiling),
      outGain: num("outGain", t.outGain),
    };
  }

  if (m.type === "cd") {
    const t = computeCdMacroTargets(p);
    return {
      ...t,
      softClip: bool("softClip", true),
      carComp: clamp01(num("carComp", t.carComp ?? 0)),
      mode: str("mode", t.mode) || t.mode,
      errorRate: clamp01(num("errorRate", t.errorRate)),
      burstMs: num("burstMs", t.burstMs),
      repeatMs: num("repeatMs", t.repeatMs),
      scratchRate: clamp01(num("scratchRate", t.scratchRate)),
      scratchAmt: clamp01(num("scratchAmt", t.scratchAmt)),
      jitterMs: num("jitterMs", t.jitterMs),
      jitterRate: num("jitterRate", t.jitterRate),
      hfLoss: clamp01(num("hfLoss", t.hfLoss)),
      servoNoise: clamp01(num("servoNoise", t.servoNoise)),
      ceiling: num("ceiling", t.ceiling),
      outGain: num("outGain", t.outGain),
    };
  }

  if (m.type === "camcorder") {
    const t = computeCamcorderMacroTargets(p);
    return {
      ...t,
      coverage: clamp01(num("coverage", 0.35)),
      movement: clamp01(num("movement", 0.25)),
      corruption: clamp01(num("corruption", 0.18)),
      agc: clamp01(num("agc", 0.35)),
      wind: bool("wind", t.wind ?? false),
      windLevel: num("windLevel", t.windLevel ?? 0.95),
      camLevel: clamp01(num("camLevel", t.camLevel ?? 0.35)),
      windBedLevel: clamp01(num("windBedLevel", t.windBedLevel ?? 0.85)),
      windHitLevel: clamp01(num("windHitLevel", t.windHitLevel ?? 0.65)),
      windHitRate: clamp01(num("windHitRate", t.windHitRate ?? 0.35)),
      camBedSource: str("camBedSource", "") || "",
      windBedSource: str("windBedSource", "") || "",
      windHitSource: str("windHitSource", "") || "",
      hpHz: num("hpHz", t.hpHz),
      lpHz: num("lpHz", t.lpHz),
      boxDb: num("boxDb", t.boxDb),
      boxHz: num("boxHz", t.boxHz),
      agcAmt: clamp01(num("agcAmt", t.agcAmt)),
      agcSpeed: clamp01(num("agcSpeed", t.agcSpeed)),
      clip: clamp01(num("clip", t.clip)),
      crush: clamp01(num("crush", t.crush)),
      bits: num("bits", t.bits),
      rate: num("rate", t.rate),
      drop: clamp01(num("drop", t.drop)),
      dropMs: num("dropMs", t.dropMs),
      dropMode: str("dropMode", t.dropMode) || t.dropMode,
      repeatMs: num("repeatMs", t.repeatMs),
      chirp: clamp01(num("chirp", t.chirp)),
      handling: clamp01(num("handling", t.handling)),
      rub: clamp01(num("rub", t.rub)),
      hiss: clamp01(num("hiss", t.hiss)),
      ceiling: num("ceiling", t.ceiling),
      outGain: num("outGain", t.outGain),
    };
  }

  return {};
}

function makeModuleEl(m, index) {
  const root = document.createElement("div");
  root.className = "module";
  root.dataset.type = m.type || "";
  const computed = settingsForModule(m);
  const controlEls = new Map(); // key -> { kind, input, valEl, fmt }

  const setParamAndUI = (key, value) => {
    if (!m.params) m.params = {};
    m.params[key] = value;
    const ce = controlEls.get(key);
    if (!ce) return;
    if (ce.kind === "slider") {
      ce.input.value = String(value);
      ce.valEl.textContent = ce.fmt(Number(value));
      return;
    }
    if (ce.kind === "toggle") {
      ce.input.checked = Boolean(value);
      ce.valEl.textContent = ce.input.checked ? "On" : "Off";
      return;
    }
    if (ce.kind === "select") {
      ce.input.value = String(value);
      ce.valEl.textContent = ce.input.value;
    }
  };

  const syncDerivedFromMacros = (changedKey) => {
    if (m.type === "occlusion") {
      if (!["distance", "wall", "material", "sourceRoom", "listenerRoom"].includes(changedKey)) return false;
      const t = computeOcclusionMacroTargets({
        distance: m.params?.distance ?? computed.distance ?? 0.35,
        wall: m.params?.wall ?? computed.wall ?? 0.45,
        material: m.params?.material ?? computed.material ?? "drywall",
        sourceRoom: m.params?.sourceRoom ?? computed.sourceRoom ?? 0.35,
        listenerRoom: m.params?.listenerRoom ?? computed.listenerRoom ?? 0.45,
      });
      for (const [k, v] of Object.entries(t)) {
        if (k === "material") continue;
        setParamAndUI(k, v);
      }
      return true;
    }

    if (m.type === "transmission") {
      if (!["bandwidth", "drive", "badConnection", "noiseProfile", "pinkNoise"].includes(changedKey)) return false;
      const bw = clamp01(Number(m.params?.bandwidth ?? computed.bandwidth ?? 0.45));
      const drive = clamp01(Number(m.params?.drive ?? computed.drive ?? 0.35));
      const bad = clamp01(Number(m.params?.badConnection ?? computed.badConnection ?? 0.25));
      const noiseProfile = clamp01(Number(m.params?.noiseProfile ?? computed.noiseProfile ?? 0.2));
      const pinkNoise = Boolean(m.params?.pinkNoise ?? computed.pinkNoise ?? false);

      if (changedKey === "bandwidth") {
        const { hp, lp, midGainDb, midQ, midFreq } = mapBandwidth(bw);
        setParamAndUI("hpHz", Math.round(hp));
        setParamAndUI("lpHz", Math.round(lp));
        setParamAndUI("midGainDb", midGainDb);
        setParamAndUI("midQ", midQ);
        setParamAndUI("midFreq", midFreq);
        setParamAndUI("boxDipDb", (1 - bw) * 2.2);
      }

      if (changedKey === "drive") {
        setParamAndUI("asym", drive * 0.6);
        setParamAndUI("comp", 0.18 + drive * 0.65);
        setParamAndUI("preDrive", Math.pow(drive, 0.85) * 0.75);
        setParamAndUI("postDrive", drive);
      }

      if (changedKey === "badConnection") {
        setParamAndUI("wowDepth", bad);
        setParamAndUI("dropRate", bad);
        setParamAndUI("dropDepth", bad);
        setParamAndUI("crackle", bad);
        setParamAndUI("lfoRate", 0.45 + bad * 1.6);
      }

      if (changedKey === "noiseProfile" || changedKey === "pinkNoise") {
        setParamAndUI("hiss", noiseProfile * 0.95);
        const autoColor = Math.max(0, (noiseProfile - 0.55) * 2);
        setParamAndUI("noiseColor", pinkNoise ? 1 : autoColor);
      }

      return true;
    }

    if (m.type === "comms") {
      if (!["mode", "bandwidth", "drive", "glitch", "noise"].includes(changedKey)) return false;
      const t = computeCommsMacroTargets({
        mode: m.params?.mode ?? computed.mode ?? "landline",
        bandwidth: m.params?.bandwidth ?? computed.bandwidth ?? 0.4,
        drive: m.params?.drive ?? computed.drive ?? 0.35,
        glitch: m.params?.glitch ?? computed.glitch ?? 0.2,
        noise: m.params?.noise ?? computed.noise ?? 0.18,
      });
      for (const [k, v] of Object.entries(t)) setParamAndUI(k, v);
      return true;
    }

    if (m.type === "conference") {
      if (!["mode", "bandwidth", "codec", "dropouts", "jitter", "robot", "noise"].includes(changedKey)) return false;
      const t = computeConferenceMacroTargets({
        mode: m.params?.mode ?? computed.mode ?? "discord",
        bandwidth: m.params?.bandwidth ?? computed.bandwidth ?? 0.45,
        codec: m.params?.codec ?? computed.codec ?? 0.35,
        dropouts: m.params?.dropouts ?? computed.dropouts ?? 0.25,
        jitter: m.params?.jitter ?? computed.jitter ?? 0.2,
        robot: m.params?.robot ?? computed.robot ?? 0.12,
        noise: m.params?.noise ?? computed.noise ?? 0.12,
      });
      for (const [k, v] of Object.entries(t)) setParamAndUI(k, v);
      return true;
    }

    if (m.type === "tape") {
      if (!["quality", "age", "wow", "glitch"].includes(changedKey)) return false;
      const t = computeTapeMacroTargets({
        quality: m.params?.quality ?? computed.quality ?? 0.55,
        age: m.params?.age ?? computed.age ?? 0.35,
        wow: m.params?.wow ?? computed.wow ?? 0.25,
        glitch: m.params?.glitch ?? computed.glitch ?? 0.18,
      });
      for (const [k, v] of Object.entries(t)) setParamAndUI(k, v);
      return true;
    }

    if (m.type === "television") {
      if (!["vibe", "speaker", "agc", "static", "hum", "whine"].includes(changedKey)) return false;
      const vibe = m.params?.vibe ?? computed.vibe ?? 0.45;
      const speaker = m.params?.speaker ?? computed.speaker ?? 0.55;
      const agc = m.params?.agc ?? computed.agc ?? 0.22;
      const staticAmt = m.params?.static ?? computed.static ?? 0.12;
      const hum = m.params?.hum ?? computed.hum ?? 0.18;
      const whine = m.params?.whine ?? computed.whine ?? 0.08;
      const t = computeTelevisionMacroTargets({ vibe, speaker, agc, static: staticAmt, hum, whine });
      setParamAndUI("hpHz", Math.round(t.hpHz));
      setParamAndUI("lpHz", Math.round(t.lpHz));
      setParamAndUI("midHumpDb", t.midHumpDb);
      setParamAndUI("midFreq", Math.round(t.midFreq));
      setParamAndUI("noiseHiss", t.noiseHiss);
      setParamAndUI("noiseCrackle", t.noiseCrackle);
      return true;
    }

    if (m.type === "cartridge") {
      if (!["quality", "codec", "grit"].includes(changedKey)) return false;
      const t = computeCartridgeMacroTargets({
        quality: m.params?.quality ?? computed.quality ?? 0.55,
        codec: m.params?.codec ?? computed.codec ?? 0.25,
        grit: m.params?.grit ?? computed.grit ?? 0.25,
        noise: m.params?.noise ?? computed.noise ?? 0.15,
      });
      for (const [k, v] of Object.entries(t)) setParamAndUI(k, v);
      return true;
    }

    if (m.type === "cd") {
      if (!["clarity", "damage", "tracking", "jitter"].includes(changedKey)) return false;
      const t = computeCdMacroTargets({
        clarity: m.params?.clarity ?? computed.clarity ?? 0.65,
        damage: m.params?.damage ?? computed.damage ?? 0.25,
        tracking: m.params?.tracking ?? computed.tracking ?? 0.22,
        jitter: m.params?.jitter ?? computed.jitter ?? 0.18,
      });
      for (const [k, v] of Object.entries(t)) setParamAndUI(k, v);
      return true;
    }

    if (m.type === "camcorder") {
      if (!["coverage", "movement", "corruption", "agc"].includes(changedKey)) return false;
      const t = computeCamcorderMacroTargets({
        coverage: m.params?.coverage ?? computed.coverage ?? 0.35,
        movement: m.params?.movement ?? computed.movement ?? 0.25,
        corruption: m.params?.corruption ?? computed.corruption ?? 0.18,
        agc: m.params?.agc ?? computed.agc ?? 0.35,
      });
      for (const [k, v] of Object.entries(t)) setParamAndUI(k, v);
      return true;
    }

    return false;
  };

  const head = document.createElement("div");
  head.className = "moduleHead";

  const title = document.createElement("div");
  title.className = "moduleTitle";
  const name = document.createElement("div");
  name.className = "name";
  name.textContent = m.name;
  const desc = document.createElement("div");
  desc.className = "desc";
  desc.textContent = m.desc;
  title.appendChild(name);
  title.appendChild(desc);

  const btns = document.createElement("div");
  btns.className = "moduleBtns";
  const left = document.createElement("button");
  left.className = "miniBtn";
  left.textContent = "←";
  left.disabled = index === 0;
  left.addEventListener("click", () => {
    if (index <= 0) return;
    [modules[index - 1], modules[index]] = [modules[index], modules[index - 1]];
    graphStale = true;
    renderModules();
    stopPlayback();
  });
  const right = document.createElement("button");
  right.className = "miniBtn";
  right.textContent = "→";
  right.disabled = index === modules.length - 1;
  right.addEventListener("click", () => {
    if (index >= modules.length - 1) return;
    [modules[index + 1], modules[index]] = [modules[index], modules[index + 1]];
    graphStale = true;
    renderModules();
    stopPlayback();
  });
  btns.appendChild(left);
  btns.appendChild(right);

  head.appendChild(title);
  head.appendChild(btns);
  root.appendChild(head);

  const row = document.createElement("div");
  row.className = "row";
  const en = document.createElement("label");
  en.className = "toggle";
  const enInput = document.createElement("input");
  enInput.type = "checkbox";
  enInput.checked = Boolean(m.enabled);
  enInput.addEventListener("change", async () => {
    m.enabled = Boolean(enInput.checked);
    graphStale = true;
    await teardownRealtime();
    renderModules();
    setState(audioBuffer ? "Ready" : "Idle");
  });
  const enSpan = document.createElement("span");
  enSpan.textContent = "Enable";
  en.appendChild(enInput);
  en.appendChild(enSpan);

  const wetWrap = document.createElement("div");
  wetWrap.className = "control";
  wetWrap.style.flex = "1";
  wetWrap.style.padding = "8px 10px";
  wetWrap.style.margin = "0";
  const wetLabelRow = document.createElement("div");
  wetLabelRow.className = "labelRow";
  wetLabelRow.style.marginBottom = "6px";
  const wetLabel = document.createElement("label");
  wetLabel.textContent = "Wet";
  const wetVal = document.createElement("span");
  wetVal.className = "val";
  wetVal.textContent = pct01(m.wet);
  wetLabelRow.appendChild(wetLabel);
  wetLabelRow.appendChild(wetVal);
  const wet = document.createElement("input");
  wet.type = "range";
  wet.min = "0";
  wet.max = "1";
  wet.step = "0.001";
  wet.value = String(m.wet ?? 1);
  wet.addEventListener("input", () => {
    m.wet = clamp01(parseFloat(wet.value));
    wetVal.textContent = pct01(m.wet);
    applyRealtimeSettings();
  });
  wetWrap.appendChild(wetLabelRow);
  wetWrap.appendChild(wet);

  row.appendChild(en);
  row.appendChild(wetWrap);
  root.appendChild(row);

  const grid = document.createElement("div");
  grid.className = "grid";

  const hasParam = (key) => Object.prototype.hasOwnProperty.call(m.params || {}, key);

  const addSliderTo = (container, key, label, { min = 0, max = 1, step = 0.001, fmt = pct01 } = {}) => {
    const c = document.createElement("div");
    c.className = "control";
    const lr = document.createElement("div");
    lr.className = "labelRow";
    const l = document.createElement("label");
    l.textContent = label;
    const v = document.createElement("span");
    v.className = "val";
    const input = document.createElement("input");
    input.type = "range";
    input.min = String(min);
    input.max = String(max);
    input.step = String(step);
    const cur = hasParam(key) ? m.params[key] : computed?.[key];
    input.value = String(cur ?? (min + max) * 0.5);
    v.textContent = fmt(parseFloat(input.value));
    controlEls.set(key, { kind: "slider", input, valEl: v, fmt });
    input.addEventListener("input", () => {
      setParamAndUI(key, parseFloat(input.value));
      if (m.type === "transmission" && key === "passes") {
        graphStale = true;
        stopPlayback();
      }
      syncDerivedFromMacros(key);
      applyRealtimeSettings();
    });
    lr.appendChild(l);
    lr.appendChild(v);
    c.appendChild(lr);
    c.appendChild(input);
    container.appendChild(c);
  };

  const addToggleTo = (container, key, label, { onChange } = {}) => {
    const c = document.createElement("div");
    c.className = "control";
    const lr = document.createElement("div");
    lr.className = "labelRow";
    const l = document.createElement("label");
    l.textContent = label;
    const v = document.createElement("span");
    v.className = "val";
    const inp = document.createElement("input");
    inp.type = "checkbox";
    inp.checked = hasParam(key) ? Boolean(m.params[key]) : Boolean(computed?.[key]);
    v.textContent = inp.checked ? "On" : "Off";
    controlEls.set(key, { kind: "toggle", input: inp, valEl: v, fmt: null });
    inp.addEventListener("change", async () => {
      setParamAndUI(key, Boolean(inp.checked));
      if (onChange) {
        try {
          await onChange(Boolean(inp.checked));
        } catch (e) {
          console.warn(e);
        }
      }
      syncDerivedFromMacros(key);
      applyRealtimeSettings();
    });
    lr.appendChild(l);
    lr.appendChild(v);
    c.appendChild(lr);
    c.appendChild(inp);
    container.appendChild(c);
  };

  const addSelectTo = (container, key, label, options, { onChange } = {}) => {
    const c = document.createElement("div");
    c.className = "control";
    const lr = document.createElement("div");
    lr.className = "labelRow";
    const l = document.createElement("label");
    l.textContent = label;
    const v = document.createElement("span");
    v.className = "val";
    const sel = document.createElement("select");
    for (const { value, text } of options) {
      const o = document.createElement("option");
      o.value = value;
      o.textContent = text;
      sel.appendChild(o);
    }
    sel.value = hasParam(key) ? String(m.params[key]) : String(computed?.[key] ?? options[0]?.value ?? "");
    v.textContent = sel.value;
    controlEls.set(key, { kind: "select", input: sel, valEl: v, fmt: null });
    sel.addEventListener("change", async () => {
      setParamAndUI(key, sel.value);
      if (onChange) await onChange(sel.value);
      syncDerivedFromMacros(key);
      applyRealtimeSettings();
    });
    lr.appendChild(l);
    lr.appendChild(v);
    c.appendChild(lr);
    c.appendChild(sel);
    container.appendChild(c);
  };

  const addSlider = (key, label, opts) => addSliderTo(grid, key, label, opts);
  const addToggle = (key, label, opts) => addToggleTo(grid, key, label, opts);
  const addSelect = (key, label, options, opts) => addSelectTo(grid, key, label, options, opts);

  const enginePresets = ENGINE_PRESETS[m.type] || null;
  const presetKeys = enginePresets ? Object.keys(enginePresets) : [];
  const userPresets = userEnginePresetsForType(m.type);
  const userKeys = Object.keys(userPresets || {}).sort((a, b) => a.localeCompare(b));

  if (presetKeys.length || userKeys.length) {
    const c = document.createElement("div");
    c.className = "control";
    const lr = document.createElement("div");
    lr.className = "labelRow";
    const l = document.createElement("label");
    l.textContent = "Preset";
    const v = document.createElement("span");
    v.className = "val";
    const sel = document.createElement("select");
    const base = document.createElement("option");
    base.value = "";
    base.textContent = "(manual)";
    sel.appendChild(base);

    if (presetKeys.length) {
      const grp = document.createElement("optgroup");
      grp.label = "Built-in";
      for (const k of presetKeys) {
        const o = document.createElement("option");
        o.value = k;
        o.textContent = presetLabelFromKey(k);
        grp.appendChild(o);
      }
      sel.appendChild(grp);
    }

    if (userKeys.length) {
      const grp = document.createElement("optgroup");
      grp.label = "Yours";
      for (const id of userKeys) {
        const p = userPresets[id];
        const o = document.createElement("option");
        o.value = `user:${id}`;
        o.textContent = String(p?.name || id);
        grp.appendChild(o);
      }
      sel.appendChild(grp);
    }

    sel.value = String(m.presetKey || "");
    const syncLabel = () => {
      v.textContent = sel.value ? String(sel.selectedOptions?.[0]?.textContent || presetLabelFromKey(sel.value)) : "Manual";
    };
    syncLabel();

    const btns = document.createElement("div");
    btns.className = "moduleBtns";
    const saveBtn = document.createElement("button");
    saveBtn.type = "button";
    saveBtn.className = "miniBtn";
    saveBtn.textContent = "Save…";
    const delBtn = document.createElement("button");
    delBtn.type = "button";
    delBtn.className = "miniBtn";
    delBtn.textContent = "Delete";

    const syncDel = () => {
      delBtn.disabled = !String(sel.value || "").startsWith("user:");
    };
    syncDel();

    sel.addEventListener("change", async () => {
      const key = String(sel.value || "");
      syncDel();
      if (!key) {
        m.presetKey = "";
        syncLabel();
        return;
      }
      syncLabel();
      await applyEnginePresetToModule(m, key);
    });

    saveBtn.addEventListener("click", () => {
      const current = String(sel.value || "");
      const defaultName = current.startsWith("user:") ? current.slice("user:".length) : "my-preset";
      const name = window.prompt(`Save ${m.name} preset name:`, defaultName);
      if (!name) return;
      const id = slugifyId(name);
      const existing = userEnginePresetsForType(m.type)[id];
      if (existing && !window.confirm(`Overwrite preset "${existing?.name || id}"?`)) return;
      setUserEnginePreset(m.type, id, { name: String(name), params: { ...(m.params || {}) } });
      m.presetKey = `user:${id}`;
      renderModules();
      applyRealtimeSettings();
    });

    delBtn.addEventListener("click", () => {
      const current = String(sel.value || "");
      if (!current.startsWith("user:")) return;
      const id = current.slice("user:".length);
      const existing = userEnginePresetsForType(m.type)[id];
      if (!window.confirm(`Delete preset "${existing?.name || id}"?`)) return;
      deleteUserEnginePreset(m.type, id);
      m.presetKey = "";
      renderModules();
      applyRealtimeSettings();
    });

    lr.appendChild(l);
    lr.appendChild(v);
    c.appendChild(lr);
    c.appendChild(sel);
    btns.appendChild(saveBtn);
    btns.appendChild(delBtn);
    c.appendChild(btns);
    grid.appendChild(c);
  }

  const advDetails = document.createElement("details");
  const advSummary = document.createElement("summary");
  advSummary.textContent = "Advanced";
  advDetails.appendChild(advSummary);
  const adv = document.createElement("div");
  adv.className = "grid";
  advDetails.appendChild(adv);

  if (m.type === "occlusion") {
    addSlider("distance", "Distance");
    addSlider("wall", "Wall Thickness");
    addSelect("material", "Material", [
      { value: "drywall", text: "Drywall" },
      { value: "wood", text: "Wood" },
      { value: "brick", text: "Brick" },
      { value: "curtain", text: "Curtain" },
      { value: "door", text: "Door" },
      { value: "glass", text: "Glass" },
    ]);
    addSlider("sourceRoom", "Source Space");
    addSlider("listenerRoom", "Listener Space");

    addSliderTo(adv, "hpHz", "High-pass", { min: 10, max: 600, step: 1, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "lpHz", "Low-pass", { min: 800, max: 18000, step: 10, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "leak", "Leak");
    addSliderTo(adv, "roomMix", "Room Mix");
    addSliderTo(adv, "predelayMs", "Pre-delay", { min: 0, max: 80, step: 1, fmt: (x) => `${Math.round(x)} ms` });
    addSliderTo(adv, "outGain", "Output", { min: 0, max: 1.5, step: 0.01, fmt: (x) => `${x.toFixed(2)}x` });
  } else if (m.type === "transmission") {
    addSlider("bandwidth", "AM Bandwidth");
    addSlider("drive", "Drive / Age");
    addSlider("badConnection", "Bad Connection");
    addSlider("noiseProfile", "Noise Profile");
    addToggle("walkieMode", "Walkie Mode");
    addSelect("walkieFx", "Walkie FX", [
      { value: "dispatch", text: "Dispatch Beep" },
      { value: "click", text: "Click/Pop" },
    ]);
    addSlider("passes", "Stack Passes", { min: 1, max: 6, step: 1, fmt: (x) => `${Math.round(x)}x` });
    addToggle("tuningEnable", "Tuning Noise", {
      onChange: async (enabled) => {
        if (!enabled) return;
        const src = String(m.params?.tuningSource || "synth");
        if (src === "synth") return;
        if (!realtime.graph) return;
        const sample = await loadTuningSample(src);
        const w = realtime.graph.modules.get(m.instanceId);
        if (w && sample) w.setTuningSample(sample);
      },
    });
    addSelect("tuningMode", "Tuning Mode", [
      { value: "edges", text: "Edges" },
      { value: "search", text: "Search" },
    ]);
    const sampleOptions = [{ value: "synth", text: "Synth" }].concat(
      (tuningManifest.samples || []).map((f) => ({ value: `file:${f}`, text: f })),
    );
    addSelect("tuningSource", "Tuning Source", sampleOptions, {
      onChange: async (val) => {
        if (!realtime.graph) return;
        try {
          const sample = await loadTuningSample(val);
          const w = realtime.graph.modules.get(m.instanceId);
          if (w && sample) w.setTuningSample(sample);
        } catch (e) {
          console.warn(e);
        }
      },
    });
    addSlider("tuningAmount", "Tuning Amount");
    addSlider("tuningCutDepth", "Tuning Cut");
    addSlider("tuningSnippetMs", "Snippet", { min: 40, max: 500, step: 1, fmt: (x) => `${Math.round(x)} ms` });

    addSliderTo(adv, "hpHz", "High-pass", { min: 40, max: 1200, step: 1, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "lpHz", "Low-pass", { min: 1200, max: 12000, step: 1, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "midGainDb", "Mid Bump", { min: -6, max: 8, step: 0.1, fmt: (x) => `${x.toFixed(1)} dB` });
    addSliderTo(adv, "midFreq", "Mid Freq", { min: 600, max: 3500, step: 1, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "midQ", "Mid Q", { min: 0.4, max: 5, step: 0.01, fmt: (x) => `${x.toFixed(2)} Q` });
    addSliderTo(adv, "boxDipDb", "Box Dip", { min: 0, max: 6, step: 0.1, fmt: (x) => `${x.toFixed(1)} dB` });
    addSliderTo(adv, "preDrive", "Pre Drive");
    addSliderTo(adv, "postDrive", "Post Drive");
    addSliderTo(adv, "asym", "Asymmetry");
    addSliderTo(adv, "comp", "Compression");
    addSliderTo(adv, "crush", "Crush");
    addSliderTo(adv, "wowDepth", "Wow Depth");
    addSliderTo(adv, "dropRate", "Drop Rate");
    addSliderTo(adv, "dropDepth", "Drop Depth");
    addSliderTo(adv, "crackle", "Crackle");
    addSliderTo(adv, "lfoRate", "LFO Rate", { min: 0.1, max: 6, step: 0.01, fmt: (x) => `${x.toFixed(2)} Hz` });
    addToggleTo(adv, "pinkNoise", "Pink Noise");
    addSliderTo(adv, "noiseColor", "Noise Color");
    addSliderTo(adv, "hiss", "Hiss");
    addSliderTo(adv, "walkieThresholdDb", "Silence Thresh", { min: -80, max: -20, step: 0.5, fmt: (x) => `${x.toFixed(1)} dB` });
    addSliderTo(adv, "walkieMinSilenceMs", "Min Silence", { min: 80, max: 600, step: 1, fmt: (x) => `${Math.round(x)} ms` });
    addSliderTo(adv, "walkieClickMs", "Click Length", { min: 5, max: 25, step: 0.5, fmt: (x) => `${x.toFixed(1)} ms` });
    addSliderTo(adv, "walkieClickLevel", "Click Level");
    addSliderTo(adv, "outGain", "Output Gain", { min: 0, max: 1.5, step: 0.01, fmt: (x) => `${x.toFixed(2)}x` });
  } else if (m.type === "comms") {
    addSelect("mode", "Mode", [
      { value: "landline", text: "Landline" },
      { value: "cell", text: "Cell" },
      { value: "intercom", text: "Intercom" },
      { value: "pa", text: "PA" },
      { value: "alarm", text: "Alarm" },
    ]);
    addSlider("bandwidth", "Bandwidth");
    addSlider("drive", "AGC / Drive");
    addSlider("glitch", "Digital Glitch");
    addSlider("noise", "Noise / Hum");
    addToggle("alarmTone", "Alarm Tone");

    addSliderTo(adv, "hpHz", "High-pass", { min: 80, max: 900, step: 1, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "lpHz", "Low-pass", { min: 1200, max: 9000, step: 10, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "midHumpDb", "Mid Hump", { min: 0, max: 12, step: 0.05, fmt: (x) => `${x.toFixed(2)} dB` });
    addSliderTo(adv, "midFreq", "Mid Freq", { min: 600, max: 5000, step: 1, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "comp", "Compression");
    addSliderTo(adv, "bits", "Bits", { min: 6, max: 16, step: 1, fmt: (x) => `${Math.round(x)}-bit` });
    addSliderTo(adv, "rate", "Rate", { min: 8000, max: 48000, step: 100, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "packet", "Packet Loss");
    addSliderTo(adv, "packetMs", "Packet Size", { min: 6, max: 120, step: 1, fmt: (x) => `${Math.round(x)} ms` });
    addSliderTo(adv, "hum", "Hum Level");
    addSliderTo(adv, "hiss", "Hiss Level");
    addSliderTo(adv, "toneMix", "Tone Mix");
    addSliderTo(adv, "ceiling", "Ceiling", { min: 0.2, max: 1, step: 0.001, fmt: pct01 });
    addSliderTo(adv, "outGain", "Output Gain", { min: 0, max: 1.5, step: 0.01, fmt: (x) => `${x.toFixed(2)}x` });

    addSliderTo(adv, "echoMix", "Echo Mix");
    addSliderTo(adv, "echoMs", "Echo Time", { min: 25, max: 900, step: 1, fmt: (x) => `${Math.round(x)} ms` });
    addSliderTo(adv, "echoFb", "Echo Feedback", { min: 0, max: 0.88, step: 0.001, fmt: pct01 });
    addSliderTo(adv, "echoTone", "Echo Tone");
    addSliderTo(adv, "verbMix", "Room Mix");
    addSliderTo(adv, "verbMs", "Room Size", { min: 35, max: 1200, step: 1, fmt: (x) => `${Math.round(x)} ms` });
    addSliderTo(adv, "verbDamp", "Room Damp");
  } else if (m.type === "conference") {
    addSelect("mode", "App Mode", [
      { value: "discord", text: "Discord" },
      { value: "zoom", text: "Zoom" },
      { value: "skype", text: "Skype" },
      { value: "cell", text: "Cell/Hotspot" },
    ]);
    addSlider("bandwidth", "Bandwidth");
    addSlider("codec", "Codec / Quality");
    addSlider("dropouts", "Dropouts");
    addSlider("jitter", "Jitter");
    addSlider("robot", "Robot / Stutter");
    addSlider("noise", "Noise");

    addSliderTo(adv, "hpHz", "High-pass", { min: 80, max: 900, step: 1, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "lpHz", "Low-pass", { min: 1200, max: 12000, step: 10, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "midHumpDb", "Presence", { min: 0, max: 12, step: 0.05, fmt: (x) => `${x.toFixed(2)} dB` });
    addSliderTo(adv, "midFreq", "Presence Freq", { min: 900, max: 4200, step: 10, fmt: (x) => `${Math.round(x)} Hz` });

    addSelectTo(adv, "concealMode", "Conceal Mode", [
      { value: "hold", text: "Hold" },
      { value: "mute", text: "Mute" },
      { value: "interp", text: "Interpolate" },
      { value: "repeat", text: "Repeat Frames" },
    ]);
    addSliderTo(adv, "packetLoss", "Packet Loss");
    addSliderTo(adv, "packetMs", "Packet Size", { min: 8, max: 180, step: 1, fmt: (x) => `${Math.round(x)} ms` });
    addSliderTo(adv, "repeatMs", "Repeat Depth", { min: 6, max: 240, step: 1, fmt: (x) => `${Math.round(x)} ms` });
    addSliderTo(adv, "jitterMs", "Jitter Depth", { min: 0, max: 2, step: 0.01, fmt: (x) => `${Number(x).toFixed(2)} ms` });
    addSliderTo(adv, "jitterRate", "Jitter Rate", { min: 6, max: 140, step: 1, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "gate", "Noise Gate");
    addSliderTo(adv, "bits", "Bit Depth", { min: 4, max: 16, step: 1, fmt: (x) => `${Math.round(x)}-bit` });
    addSliderTo(adv, "rate", "Sample Rate", { min: 6000, max: 48000, step: 100, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "ceiling", "Ceiling", { min: 0.2, max: 1, step: 0.001, fmt: pct01 });
    addSliderTo(adv, "outGain", "Output Gain", { min: 0, max: 1.5, step: 0.01, fmt: (x) => `${x.toFixed(2)}x` });
  } else if (m.type === "tape") {
    addSlider("quality", "Quality");
    addSlider("age", "Age / Drive");
    addSlider("wow", "Wow/Flutter");
    addSlider("glitch", "Dropouts");
    addToggle("sfxEnable", "Tape Noise SFX");
    addSelect(
      "sfxSource",
      "SFX Source",
      [{ value: "", text: "(none)" }].concat((tapeSfxManifest.banks || []).map((b) => ({ value: b.id, text: b.name || b.id }))),
    );

    addSliderTo(adv, "hpHz", "High-pass", { min: 10, max: 240, step: 1, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "lpHz", "Low-pass", { min: 1800, max: 18000, step: 10, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "headBumpDb", "Head Bump", { min: 0, max: 10, step: 0.05, fmt: (x) => `${x.toFixed(2)} dB` });
    addSliderTo(adv, "headBumpHz", "Bump Freq", { min: 40, max: 160, step: 1, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "drive", "Saturation");
    addSliderTo(adv, "comp", "Compression");
    addSliderTo(adv, "speed", "Speed", { min: 0.85, max: 1.15, step: 0.001, fmt: (x) => `${x.toFixed(3)}x` });
    addSliderTo(adv, "wowDepthMs", "Wow Depth", { min: 0, max: 14, step: 0.1, fmt: (x) => `${x.toFixed(1)} ms` });
    addSliderTo(adv, "flutterDepthMs", "Flutter Depth", { min: 0, max: 6, step: 0.05, fmt: (x) => `${x.toFixed(2)} ms` });
    addSliderTo(adv, "hiss", "Hiss");
    addSliderTo(adv, "hum", "Hum");
    addSliderTo(adv, "dropout", "Dropout Amount");
    addSliderTo(adv, "dropoutMs", "Dropout Length", { min: 8, max: 220, step: 1, fmt: (x) => `${Math.round(x)} ms` });
    addSliderTo(adv, "ceiling", "Ceiling", { min: 0.2, max: 1, step: 0.001, fmt: pct01 });
    addSliderTo(adv, "outGain", "Output Gain", { min: 0, max: 1.5, step: 0.01, fmt: (x) => `${x.toFixed(2)}x` });
    addSliderTo(adv, "sfxLevel", "SFX Level");
    addSelectTo(adv, "sfxMode", "SFX Mode", [
      { value: "bed", text: "Loop Bed" },
      { value: "edges", text: "Start/End Only" },
      { value: "sequence", text: "Sequence (Start+Bed+End)" },
    ]);
  } else if (m.type === "television") {
    addSlider("vibe", "Vibe");
    addSlider("speaker", "Speaker");
    addSlider("agc", "AGC");
    addSlider("static", "Static");
    addSlider("hum", "Hum");
    addSlider("whine", "Whine");
    addToggle("bedEnable", "CRT Bed", { onChange: async () => syncTelevisionBedRuntime() });
    addSlider("bedLevel", "Bed Level");

    const bedSelOpts = (list, { noneText, emptyText }) =>
      [{ value: "", text: list.length ? noneText : emptyText }].concat(list.map((n) => ({ value: n, text: n })));
    addSelectTo(
      adv,
      "bedSource",
      "Bed Source",
      bedSelOpts(tvSfxManifest.beds || [], {
        noneText: "(none)",
        emptyText: "(no entries in television-engine/audio/manifest.json)",
      }),
      { onChange: async () => syncTelevisionBedRuntime() },
    );

    addSliderTo(adv, "hpHz", "High-pass", { min: 20, max: 1200, step: 1, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "lpHz", "Low-pass", { min: 800, max: 18000, step: 10, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "midHumpDb", "Mid Hump", { min: -6, max: 10, step: 0.1, fmt: (x) => `${Number(x).toFixed(1)} dB` });
    addSliderTo(adv, "midFreq", "Mid Freq", { min: 600, max: 5000, step: 1, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "noiseHiss", "Noise Hiss");
    addSliderTo(adv, "noiseCrackle", "Noise Crackle");
    addSliderTo(adv, "outGain", "Output", { min: 0, max: 1.5, step: 0.01, fmt: (x) => `${x.toFixed(2)}x` });
  } else if (m.type === "cartridge") {
    addSlider("quality", "Quality");
    addSlider("codec", "Compression");
    addSlider("grit", "Grit");
    addSlider("noise", "Noise");
    addToggle("dither", "Dither");
    addToggle("noiseShaping", "Noise Shaping");

    addToggleTo(adv, "bleepsEnable", "Bleeps/Blips");
    addSliderTo(adv, "bleepsMix", "Bleeps Mix");
    addSliderTo(adv, "bleepsRate", "Bleeps Rate", { min: 0, max: 18, step: 0.1, fmt: (x) => `${x.toFixed(1)} Hz` });
    addSelectTo(adv, "bleepsWave", "Bleeps Wave", [
      { value: "random", text: "Random" },
      { value: "pulse", text: "Pulse" },
      { value: "saw", text: "Saw" },
      { value: "tri", text: "Triangle" },
    ]);
    addSliderTo(adv, "bleepsVibrato", "Bleeps Vibrato");
    addSliderTo(adv, "bleepsPitch", "Bleeps Pitch");

    addSliderTo(adv, "microDelayMs", "Micro Delay", { min: 0, max: 30, step: 0.1, fmt: (x) => `${x.toFixed(1)} ms` });
    addSliderTo(adv, "microDelayMix", "Micro Delay Mix");
    addSliderTo(adv, "verb", "Conduction Verb");
    addSliderTo(adv, "verbMs", "Verb Size", { min: 10, max: 120, step: 1, fmt: (x) => `${Math.round(x)} ms` });

    addSliderTo(adv, "speaker", "Speaker Sim");
    addSliderTo(adv, "hpHz", "High-pass", { min: 20, max: 240, step: 1, fmt: (x) => `${Math.round(x)} Hz` });

    addSliderTo(adv, "bits", "Bit Depth", { min: 2, max: 16, step: 1, fmt: (x) => `${Math.round(x)}-bit` });
    addSliderTo(adv, "rate", "Sample Rate", { min: 6000, max: 48000, step: 100, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "jitter", "Clock Jitter");
    addSliderTo(adv, "lpHz", "Low-pass", { min: 2500, max: 18000, step: 50, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "preEmph", "Pre-Emphasis");
    addSliderTo(adv, "mulaw", "Mu-law Mix");
    addSliderTo(adv, "blockMs", "Block Hold", { min: 0, max: 60, step: 1, fmt: (x) => `${Math.round(x)} ms` });

    addSliderTo(adv, "sat", "DAC Saturation");
    addSliderTo(adv, "edge", "Edge");
    addSliderTo(adv, "dcDrift", "DC Drift");
    addSliderTo(adv, "hum", "Hum");
    addSliderTo(adv, "whine", "Bus Whine");
    addSliderTo(adv, "noiseTrack", "Noise Track");
    addSliderTo(adv, "outGain", "Output Gain", { min: 0, max: 1.5, step: 0.01, fmt: (x) => `${x.toFixed(2)}x` });

    addSliderTo(adv, "wet", "Wet");
    addSliderTo(adv, "ceiling", "Ceiling", { min: 0.2, max: 1, step: 0.001, fmt: pct01 });
    addSliderTo(adv, "limiter", "Limiter");
  } else if (m.type === "cd") {
    addSlider("clarity", "Clarity");
    addSlider("damage", "Damage");
    addSlider("tracking", "Tracking");
    addSlider("jitter", "Jitter");
    addSlider("carComp", "Car Stereo Comp");
    addToggle("softClip", "Soft Clip");

    addSelectTo(adv, "mode", "Error Mode", [
      { value: "hold", text: "Hold" },
      { value: "mute", text: "Mute" },
      { value: "interp", text: "Interp" },
      { value: "repeat", text: "Repeat" },
    ]);
    addSliderTo(adv, "errorRate", "Error Rate");
    addSliderTo(adv, "burstMs", "Burst", { min: 4, max: 260, step: 1, fmt: (x) => `${Math.round(x)} ms` });
    addSliderTo(adv, "repeatMs", "Repeat", { min: 6, max: 220, step: 1, fmt: (x) => `${Math.round(x)} ms` });
    addSliderTo(adv, "scratchRate", "Scratch Rate");
    addSliderTo(adv, "scratchAmt", "Scratch Amt");
    addSliderTo(adv, "jitterMs", "Jitter Depth", { min: 0, max: 1.4, step: 0.01, fmt: (x) => `${Number(x).toFixed(2)} ms` });
    addSliderTo(adv, "jitterRate", "Jitter Rate", { min: 8, max: 140, step: 1, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "hfLoss", "HF Loss");
    addSliderTo(adv, "servoNoise", "Servo Noise");
    addSliderTo(adv, "ceiling", "Ceiling", { min: 0.2, max: 1, step: 0.001, fmt: pct01 });
    addSliderTo(adv, "outGain", "Output", { min: 0, max: 1.5, step: 0.01, fmt: (x) => `${x.toFixed(2)}x` });
  } else if (m.type === "camcorder") {
    addSlider("coverage", "Coverage / Muffle");
    addSlider("movement", "Movement Noise");
    addSlider("corruption", "Corruption");
    addSlider("agc", "AGC Drive");
    addToggle("wind", "Wind");
    addSlider("windLevel", "Wind Level", { min: 0, max: 1.5, step: 0.001, fmt: (x) => `${Math.round(x * 100)}%` });

    addSliderTo(adv, "camLevel", "Cam Motor");
    addSliderTo(adv, "windBedLevel", "Wind Bed");
    addSliderTo(adv, "windHitLevel", "Wind Hits");
    addSliderTo(adv, "windHitRate", "Hit Rate");

    const camSelOpts = (list, { noneText, emptyText }) =>
      [{ value: "", text: list.length ? noneText : emptyText }].concat(list.map((n) => ({ value: n, text: n })));
    addSelectTo(
      adv,
      "camBedSource",
      "Cam Bed Source",
      camSelOpts(camcorderSfxManifest.camBed || [], {
        noneText: "(none)",
        emptyText: "(no entries in camcorder-engine/audio/manifest.json)",
      }),
    );
    addSelectTo(
      adv,
      "windBedSource",
      "Wind Bed Source",
      camSelOpts(camcorderSfxManifest.windBed || [], {
        noneText: "(none)",
        emptyText: "(no entries in camcorder-engine/audio/manifest.json)",
      }),
    );
    addSelectTo(
      adv,
      "windHitSource",
      "Wind Hits Source",
      camSelOpts(camcorderSfxManifest.windHits || [], {
        noneText: "(none)",
        emptyText: "(no entries in camcorder-engine/audio/manifest.json)",
      }),
    );

    if (!String(m.params?.camBedSource || "") && (camcorderSfxManifest.camBed || []).length) setParamAndUI("camBedSource", camcorderSfxManifest.camBed[0]);
    if (!String(m.params?.windBedSource || "") && (camcorderSfxManifest.windBed || []).length)
      setParamAndUI("windBedSource", camcorderSfxManifest.windBed[0]);

    addSliderTo(adv, "hpHz", "High-pass", { min: 10, max: 280, step: 1, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "lpHz", "Low-pass", { min: 1400, max: 18000, step: 10, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "boxDb", "Boxiness", { min: 0, max: 12, step: 0.05, fmt: (x) => `${Number(x).toFixed(1)} dB` });
    addSliderTo(adv, "boxHz", "Box Freq", { min: 650, max: 3200, step: 10, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "agcAmt", "AGC Amount");
    addSliderTo(adv, "agcSpeed", "AGC Speed");
    addSliderTo(adv, "clip", "Clip");
    addSliderTo(adv, "crush", "ADC Crush");
    addSliderTo(adv, "bits", "Bit Depth", { min: 6, max: 16, step: 1, fmt: (x) => `${Math.round(x)}-bit` });
    addSliderTo(adv, "rate", "Sample Rate", { min: 8000, max: 48000, step: 100, fmt: (x) => `${Math.round(x)} Hz` });
    addSliderTo(adv, "drop", "Dropouts");
    addSliderTo(adv, "dropMs", "Drop Length", { min: 6, max: 260, step: 1, fmt: (x) => `${Math.round(x)} ms` });
    addSelectTo(adv, "dropMode", "Drop Mode", [
      { value: "hold", text: "Hold" },
      { value: "mute", text: "Mute" },
      { value: "interp", text: "Interp" },
      { value: "repeat", text: "Repeat" },
    ]);
    addSliderTo(adv, "repeatMs", "Repeat Depth", { min: 8, max: 240, step: 1, fmt: (x) => `${Math.round(x)} ms` });
    addSliderTo(adv, "chirp", "Chirp Bursts");
    addSliderTo(adv, "handling", "Handling Thumps");
    addSliderTo(adv, "rub", "Rub Noise");
    addSliderTo(adv, "hiss", "Hiss");
    addSliderTo(adv, "ceiling", "Ceiling", { min: 0.2, max: 1, step: 0.001, fmt: pct01 });
    addSliderTo(adv, "outGain", "Output Gain", { min: 0, max: 1.5, step: 0.01, fmt: (x) => `${x.toFixed(2)}x` });
  }

  root.appendChild(grid);
  root.appendChild(advDetails);
  return root;
}

function renderModules() {
  els.moduleRow.replaceChildren();
  const visible = getVisibleModules();
  visible.forEach((m) => {
    const index = modules.indexOf(m);
    els.moduleRow.appendChild(makeModuleEl(m, index));
  });
  syncModuleRowColumns();
}

async function applyEnginePresetToModule(m, presetKey) {
  const presets = ENGINE_PRESETS[m.type] || null;
  if (!presets) return;
  const key = String(presetKey || "");
  const userId = key.startsWith("user:") ? key.slice("user:".length) : "";
  const userEntry = userId ? userEnginePresetsForType(m.type)[userId] : null;
  const preset = userEntry && typeof userEntry === "object" ? (userEntry.params && typeof userEntry.params === "object" ? userEntry.params : userEntry) : presets[key];
  if (!preset || typeof preset !== "object") return;

  const prevPasses = Number(m.params?.passes ?? 1);
  m.params = { ...(m.params || {}), ...preset };
  m.presetKey = key;

  const nextPasses = Number(m.params?.passes ?? 1);
  const needsRebuild = m.type === "transmission" && Number.isFinite(nextPasses) && Math.floor(nextPasses) !== Math.floor(prevPasses);
  if (needsRebuild) graphStale = true;
  if (needsRebuild && realtime.ctx) await teardownRealtime();

  if (m.type === "transmission") {
    const src = String(m.params?.tuningSource || "synth");
    const enable = Boolean(m.params?.tuningEnable);
    if (enable && src !== "synth" && realtime.graph) {
      try {
        const sample = await loadTuningSample(src);
        const w = realtime.graph.modules.get(m.instanceId);
        if (w && sample) w.setTuningSample(sample);
      } catch (e) {
        console.warn(e);
      }
    }
  }

  renderModules();
  applyRealtimeSettings();
}

async function applyMasterPresetById(presetId) {
  const id = String(presetId || "");
  if (!id) return;
  const user = id.startsWith("user:") ? getUserMasterPresets()[id.slice("user:".length)] : null;
  const preset = user || MASTER_PRESETS.find((p) => p.id === id) || null;
  if (!preset) return;
  await teardownRealtime();
  viewSnapshot = null;
  viewMode = "suite";
  if (els.viewSelect) els.viewSelect.value = "suite";

  // Master
  const mp = preset.master && typeof preset.master === "object" ? preset.master : {};
  els.masterGain.value = String(typeof mp.masterGain === "number" ? mp.masterGain : 0.25);
  if (els.masterHpHz) els.masterHpHz.value = String(typeof mp.masterHpHz === "number" ? mp.masterHpHz : 20);
  if (els.masterLpHz) els.masterLpHz.value = String(typeof mp.masterLpHz === "number" ? mp.masterLpHz : 20000);
  if (els.masterComp) els.masterComp.value = String(typeof mp.masterComp === "number" ? mp.masterComp : 0.12);
  els.ceiling.value = String(typeof mp.ceiling === "number" ? mp.ceiling : 0.92);
  els.limiter.value = String(typeof mp.limiter === "number" ? mp.limiter : 0.45);
  els.softClip.checked = typeof mp.softClip === "boolean" ? mp.softClip : true;
  els.monoOut.checked = typeof mp.monoOut === "boolean" ? mp.monoOut : true;
  refreshMasterLabels();

  // Modules
  const base = makeDefaultModules();
  const byType = new Map(base.map((m) => [m.type, m]));
  for (const [type, conf] of Object.entries(preset.modules || {})) {
    const mod = byType.get(type);
    if (!mod || !conf) continue;
    if (typeof conf.enabled === "boolean") mod.enabled = conf.enabled;
    if (typeof conf.wet === "number") mod.wet = clamp01(conf.wet);
    if (conf.params && typeof conf.params === "object") mod.params = { ...(mod.params || {}), ...conf.params };
    mod.presetKey = "";
  }

  const order = Array.isArray(preset.order) ? preset.order : base.map((m) => m.type);
  const nextModules = [];
  const seen = new Set();
  for (const t of order) {
    const mod = byType.get(t);
    if (!mod) continue;
    if (seen.has(mod.type)) continue;
    nextModules.push(mod);
    seen.add(mod.type);
  }
  for (const mod of base) {
    if (seen.has(mod.type)) continue;
    nextModules.push(mod);
    seen.add(mod.type);
  }
  modules = nextModules;

  // Output mode update
  if (sourceBuffer) {
    audioBuffer = els.monoOut.checked ? makeMonoBuffer(sourceBuffer) : makeStereoBuffer(sourceBuffer);
    refreshFileStatus(els.fileName.textContent);
  }

  graphStale = true;
  renderModules();
  setState(audioBuffer ? "Ready" : "Idle");
}

function stopPlayback() {
  stopAutomationPlayback();
  for (const t of realtime.endTimers) {
    try {
      window.clearTimeout(t);
    } catch {
      // ignore
    }
  }
  realtime.endTimers = [];

  for (const s of realtime.tvBedSources) {
    if (!s) continue;
    try {
      s.stop();
    } catch {
      // ignore
    }
    try {
      s.disconnect();
    } catch {
      // ignore
    }
  }
  realtime.tvBedSources = [];

  for (const s of realtime.extraSources) {
    if (!s) continue;
    try {
      s.stop();
    } catch {
      // ignore
    }
    try {
      s.disconnect();
    } catch {
      // ignore
    }
  }
  realtime.extraSources = [];
  realtime.playWindow = null;

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
  realtime.playing = false;
  els.stopBtn.disabled = true;
  setState(audioBuffer ? "Ready" : "Idle");
}

async function syncTelevisionBedRuntime() {
  if (!realtime.ctx || !realtime.graph) return;
  const tv = modules.find((m) => m.type === "television");

  for (const s of realtime.tvBedSources) {
    if (!s) continue;
    try {
      s.stop();
    } catch {
      // ignore
    }
    try {
      s.disconnect();
    } catch {
      // ignore
    }
  }
  realtime.tvBedSources = [];

  if (!tv?.enabled) return;
  if (!realtime.src) return; // only run during active playback
  const p = tv.params || {};
  const bedEnable = Boolean(p.bedEnable);
  const bedSource = String(p.bedSource || "");
  if (!bedEnable || !bedSource) return;

  try {
    const buf = await ensureTvBedDecoded(bedSource);
    const w = realtime.graph.modules.get(tv.instanceId);
    if (!buf || !w) return;
    const looping = Boolean(els.loopToggle.checked);
    const nowT = realtime.ctx.currentTime;

    let stopAt = null;
    const pw = realtime.playWindow;
    if (!looping && pw && Number.isFinite(pw.tapeEndStopAt) && pw.tapeEndStopAt > 0) stopAt = pw.tapeEndStopAt;
    if (!looping && (!stopAt || stopAt <= nowT) && pw && Number.isFinite(pw.baseTime) && Number.isFinite(pw.audioStartAt) && audioBuffer) {
      stopAt = pw.baseTime + pw.audioStartAt + audioBuffer.duration;
    }

    for (const lg of w.laneGraphs || []) {
      const sfxNode = lg?.graph?.nodes?.sfx || lg?.graph?.sfx || null;
      if (!sfxNode) continue;
      const bed = new AudioBufferSourceNode(realtime.ctx, { buffer: buf });
      bed.loop = true;
      bed.connect(sfxNode);
      bed.start(nowT, 0);
      if (!looping && stopAt && Number.isFinite(stopAt) && stopAt > nowT) bed.stop(stopAt);
      realtime.tvBedSources.push(bed);
    }
  } catch (e) {
    console.warn(e);
  }
}

function stopAutomationPlayback() {
  for (const s of automationRt.sources) {
    if (!s) continue;
    try {
      s.stop();
    } catch {
      // ignore
    }
    try {
      s.disconnect();
    } catch {
      // ignore
    }
  }
  automationRt.sources = [];
  automationRt.playing = false;
  automationRt.stopAt = 0;
}

async function teardownAutomationRealtime() {
  stopAutomationPlayback();
  if (automationRt.ctx) {
    try {
      await automationRt.ctx.close();
    } catch {
      // ignore
    }
  }
  automationRt.ctx = null;
  automationRt.layers = [];
  automationRt.sum = null;
  automationRt.master = null;
  automationRt.sampleRate = 0;
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
  graphStale = true;
}

function makeMonoBuffer(decoded) {
  if (decoded.numberOfChannels === 1) return decoded;
  const out = new AudioBuffer({ length: decoded.length, sampleRate: decoded.sampleRate, numberOfChannels: 1 });
  const dst = out.getChannelData(0);
  const a = decoded.getChannelData(0);
  const b = decoded.getChannelData(1);
  for (let i = 0; i < dst.length; i++) dst[i] = 0.5 * (a[i] + b[i]);
  return out;
}

function makeStereoBuffer(decoded) {
  if (decoded.numberOfChannels === 2) return decoded;
  const out = new AudioBuffer({ length: decoded.length, sampleRate: decoded.sampleRate, numberOfChannels: 2 });
  const l = out.getChannelData(0);
  const r = out.getChannelData(1);
  if (decoded.numberOfChannels === 1) {
    const src = decoded.getChannelData(0);
    l.set(src);
    r.set(src);
  } else {
    const ch0 = decoded.getChannelData(0);
    const ch1 = decoded.getChannelData(1) || decoded.getChannelData(0);
    for (let i = 0; i < decoded.length; i++) {
      l[i] = ch0[i];
      r[i] = ch1[i];
    }
  }
  return out;
}

function refreshFileStatus(name = "None") {
  els.fileName.textContent = name || "None";
  els.duration.textContent = audioBuffer ? fmtTime(audioBuffer.duration) : "-";
  els.sampleRate.textContent = audioBuffer ? `${audioBuffer.sampleRate} Hz` : "-";
}

async function ensureRealtimeGraph() {
  if (!audioBuffer) return;
  const wantStereo = !els.monoOut.checked;
  if (realtime.ctx && realtime.graph && !graphStale && realtime.stereo === wantStereo && realtime.ctx.sampleRate === audioBuffer.sampleRate) return;

  await teardownRealtime();
  realtime.stereo = wantStereo;

  realtime.ctx = new (window.AudioContext || window.webkitAudioContext)({
    latencyHint: "interactive",
    sampleRate: audioBuffer.sampleRate,
  });

  let sample = null;
  const trans = modules.find((m) => m.type === "transmission");
  if (trans?.enabled && trans.params?.tuningEnable && trans.params?.tuningSource && trans.params?.tuningSource !== "synth") {
    try {
      sample = await loadTuningSample(trans.params.tuningSource);
    } catch (e) {
      console.warn(e);
    }
  }

  realtime.graph = await buildLameGraph(realtime.ctx, {
    seed: audioDataSeed,
    modules: modules.filter((m) => Boolean(m.enabled)),
    stereo: wantStereo,
    tuningEdges,
    tuningSample: sample,
  });

  realtime.graph.output.connect(realtime.ctx.destination);
  graphStale = false;
  applyRealtimeSettings({ ramp: 0 });
}

function eqPowGains(wet01) {
  const w = clamp01(wet01);
  const a = w * Math.PI * 0.5;
  return { dry: Math.cos(a), wet: Math.sin(a) };
}

function scheduleScalarAutomation(param, lane, fallbackValue, { startTime, duration, kind = "float" } = {}) {
  if (!param) return;
  const start = Number(startTime) || 0;
  const dur = Math.max(0, Number(duration) || 0);
  const points = lane?.points?.length ? lane.points.slice().filter((p) => p.t >= 0 && p.t <= dur).sort((a, b) => a.t - b.t) : [];

  param.cancelScheduledValues(start);
  const v0 = lane ? lane.valueAt(0, fallbackValue) : fallbackValue;
  param.setValueAtTime(kind === "bool" ? (v0 >= 0.5 ? 1 : 0) : Number(v0) || 0, start);

  if (!points.length) return;
  for (const p of points) {
    const t = start + p.t;
    const v = kind === "bool" ? (p.v >= 0.5 ? 1 : 0) : Number(p.v) || 0;
    if (kind === "bool") param.setValueAtTime(v, t);
    else param.linearRampToValueAtTime(v, t);
  }
}

function scheduleWetEnabledAutomation(wrapper, { baseWet = 1, baseEnabled = true, wetLane = null, enabledLane = null, startTime = 0, duration = 0 } = {}) {
  if (!wrapper?.laneWraps?.length) return;
  const dur = Math.max(0, Number(duration) || 0);
  const times = new Set([0, dur]);
  for (const p of wetLane?.points || []) if (p && Number.isFinite(p.t) && p.t >= 0 && p.t <= dur) times.add(p.t);
  for (const p of enabledLane?.points || []) if (p && Number.isFinite(p.t) && p.t >= 0 && p.t <= dur) times.add(p.t);
  const sorted = Array.from(times).sort((a, b) => a - b);

  const evalWet = (t) => {
    const en = enabledLane ? enabledLane.valueAt(t, baseEnabled ? 1 : 0) >= 0.5 : Boolean(baseEnabled);
    const wet = wetLane ? wetLane.valueAt(t, baseWet) : baseWet;
    return en ? clamp01(wet) : 0;
  };

  for (const w of wrapper.laneWraps) {
    const dry = w?.dryGain?.gain;
    const wet = w?.wetGain?.gain;
    if (!dry || !wet) continue;
    dry.cancelScheduledValues(startTime);
    wet.cancelScheduledValues(startTime);

    const g0 = eqPowGains(evalWet(sorted[0] || 0));
    dry.setValueAtTime(g0.dry, startTime + (sorted[0] || 0));
    wet.setValueAtTime(g0.wet, startTime + (sorted[0] || 0));
    for (let i = 1; i < sorted.length; i++) {
      const t = sorted[i];
      const g = eqPowGains(evalWet(t));
      dry.linearRampToValueAtTime(g.dry, startTime + t);
      wet.linearRampToValueAtTime(g.wet, startTime + t);
    }
  }
}

function getAutomationSampleRate() {
  let sr = 0;
  for (const layer of automation.layers) {
    if (!layer?.buffer) continue;
    sr = Math.max(sr, layer.buffer.sampleRate || 0);
  }
  return sr || 44100;
}

async function ensureAutomationRealtimeGraph() {
  ensureAutomationLayers();
  const wantStereo = !els.monoOut.checked;
  const sr = getAutomationSampleRate();
  if (automationRt.ctx && automationRt.master && automationRt.sum && automationRt.stereo === wantStereo && automationRt.sampleRate === sr) return;

  await teardownAutomationRealtime();
  automationRt.stereo = wantStereo;
  automationRt.sampleRate = sr;
  automationRt.ctx = new (window.AudioContext || window.webkitAudioContext)({ latencyHint: "interactive", sampleRate: sr });

  const ctx = automationRt.ctx;
  automationRt.sum = new GainNode(ctx, { gain: 1, channelCount: wantStereo ? 2 : 1 });
  automationRt.master = buildMasterLane(ctx);
  automationRt.sum.connect(automationRt.master.input);
  automationRt.master.output.connect(ctx.destination);

  automationRt.layers = [];
  for (const layer of automation.layers) {
    const graph = await buildLameGraph(ctx, {
      seed: layer.seed >>> 0,
      modules: layer.modules,
      stereo: wantStereo,
      tuningEdges: layer.tuningEdges,
      tuningSample: null,
      withMaster: false,
    });
    const enabledNode = new GainNode(ctx, { gain: layer.enabled ? 1 : 0 });
    const gainNode = new GainNode(ctx, { gain: layer.gain ?? 1 });
    graph.output.connect(enabledNode);
    enabledNode.connect(gainNode);
    gainNode.connect(automationRt.sum);
    automationRt.layers.push({ graph, enabledNode, gainNode });
  }
}

function applyAutomationGraphSettings({ time, ramp = 0.02, allLayers = false } = {}) {
  const ctx = automationRt.ctx;
  if (!ctx || !automationRt.master) return;
  const t = Number.isFinite(time) ? time : ctx.currentTime;
  automationRt.master.applySettings(readMasterSettings(), { time: t, ramp });
  const indices = allLayers ? automation.layers.map((_, i) => i) : [automation.activeLayer | 0];
  for (const i of indices) {
    const layer = automation.layers[i];
    const rt = automationRt.layers[i];
    if (!layer || !rt) continue;
    for (const m of layer.modules) {
      const w = rt.graph.modules.get(m.instanceId);
      if (!w) continue;
      w.applySettings(settingsForModule(m), { time: t, ramp });
    }
  }
}

async function startAutomationPlayback() {
  ensureAutomationLayers();
  const any = automation.layers.some((l) => l?.buffer);
  if (!any) throw new Error("No layer audio loaded");
  await ensureAutomationRealtimeGraph();
  if (!automationRt.ctx) return;

  stopAutomationPlayback();
  await automationRt.ctx.resume();

  const ctx = automationRt.ctx;
  const baseTime = ctx.currentTime + 0.06;
  const dur = getAutomationLengthSec();
  automationRt.stopAt = baseTime + dur + 0.05;

  // Apply settings + deterministic reset.
  applyAutomationGraphSettings({ time: baseTime, ramp: 0.02, allLayers: true });
  for (const rt of automationRt.layers) rt.graph.resetAll();

  // Schedule automation (wet/enable + layer gain/enable).
  for (let li = 0; li < automation.layers.length; li++) {
    const layer = automation.layers[li];
    const rt = automationRt.layers[li];
    if (!layer || !rt) continue;

    // Layer enable/gain automation lanes.
    const enLane = layer.automations.layer.get("enable") || null;
    const gLane = layer.automations.layer.get("gain") || null;
    scheduleScalarAutomation(rt.enabledNode.gain, enLane, layer.enabled ? 1 : 0, { startTime: baseTime, duration: dur, kind: "bool" });
    scheduleScalarAutomation(rt.gainNode.gain, gLane, Number(layer.gain ?? 1), { startTime: baseTime, duration: dur, kind: "float" });

    // Per-module wet/enable automation lanes.
    for (const m of layer.modules) {
      const w = rt.graph.modules.get(m.instanceId);
      if (!w) continue;
      const entry = layer.automations.modules.get(m.instanceId) || null;
      const wetLane = entry?.wet || null;
      const enabledLane = entry?.enabled || null;
      scheduleWetEnabledAutomation(w, {
        baseWet: clamp01(m.wet ?? 1),
        baseEnabled: Boolean(m.enabled),
        wetLane,
        enabledLane,
        startTime: baseTime,
        duration: dur,
      });
    }
  }

  // Schedule sources.
  for (let li = 0; li < automation.layers.length; li++) {
    const layer = automation.layers[li];
    const rt = automationRt.layers[li];
    if (!layer?.buffer || !rt) continue;
    const buf = automationRt.stereo ? makeStereoBuffer(layer.buffer) : makeMonoBuffer(layer.buffer);
    const src = new AudioBufferSourceNode(ctx, { buffer: buf });
    src.connect(rt.graph.input);
    src.start(baseTime + Math.max(0, layer.clipStartSec));
    src.stop(baseTime + Math.max(0, layer.clipStartSec) + buf.duration);
    automationRt.sources.push(src);
  }

  automationRt.playing = true;
  els.stopBtn.disabled = false;
  setState("Playing (Automation)");

  window.setTimeout(() => {
    if (!automationRt.playing) return;
    stopAutomationPlayback();
    els.stopBtn.disabled = true;
    setState("Stopped");
  }, Math.max(0, Math.round((automationRt.stopAt - ctx.currentTime) * 1000)));
}

async function exportAutomationWav() {
  ensureAutomationLayers();
  const any = automation.layers.some((l) => l?.buffer);
  if (!any) return;
  els.exportBtn.disabled = true;
  setState("Rendering (Automation)…");
  try {
    const wantStereo = !els.monoOut.checked;
    const ch = wantStereo ? 2 : 1;
    const sr = getAutomationSampleRate();
    const dur = getAutomationLengthSec();
    const frames = Math.max(1, Math.ceil(dur * sr));
    const offline = new OfflineAudioContext(ch, frames, sr);

    const sum = new GainNode(offline, { gain: 1, channelCount: wantStereo ? 2 : 1 });
    const master = buildMasterLane(offline);
    sum.connect(master.input);
    master.output.connect(offline.destination);
    master.applySettings(readMasterSettings(), { time: 0, ramp: 0 });

    const layerGraphs = [];
    for (let li = 0; li < automation.layers.length; li++) {
      const layer = automation.layers[li];
      const graph = await buildLameGraph(offline, {
        seed: layer.seed >>> 0,
        modules: layer.modules,
        stereo: wantStereo,
        tuningEdges: layer.tuningEdges,
        tuningSample: null,
        withMaster: false,
      });
      const enabledNode = new GainNode(offline, { gain: layer.enabled ? 1 : 0 });
      const gainNode = new GainNode(offline, { gain: layer.gain ?? 1 });
      graph.output.connect(enabledNode);
      enabledNode.connect(gainNode);
      gainNode.connect(sum);
      layerGraphs.push({ graph, enabledNode, gainNode });

      // Static settings (module internals).
      for (const m of layer.modules) {
        const w = graph.modules.get(m.instanceId);
        if (!w) continue;
        w.applySettings(settingsForModule(m), { time: 0, ramp: 0 });
      }
      graph.resetAll();
    }

    // Schedule automation (wet/enable + layer gain/enable).
    for (let li = 0; li < automation.layers.length; li++) {
      const layer = automation.layers[li];
      const rt = layerGraphs[li];
      if (!layer || !rt) continue;
      const enLane = layer.automations.layer.get("enable") || null;
      const gLane = layer.automations.layer.get("gain") || null;
      scheduleScalarAutomation(rt.enabledNode.gain, enLane, layer.enabled ? 1 : 0, { startTime: 0, duration: dur, kind: "bool" });
      scheduleScalarAutomation(rt.gainNode.gain, gLane, Number(layer.gain ?? 1), { startTime: 0, duration: dur, kind: "float" });

      for (const m of layer.modules) {
        const w = rt.graph.modules.get(m.instanceId);
        if (!w) continue;
        const entry = layer.automations.modules.get(m.instanceId) || null;
        scheduleWetEnabledAutomation(w, {
          baseWet: clamp01(m.wet ?? 1),
          baseEnabled: Boolean(m.enabled),
          wetLane: entry?.wet || null,
          enabledLane: entry?.enabled || null,
          startTime: 0,
          duration: dur,
        });
      }
    }

    // Sources
    for (let li = 0; li < automation.layers.length; li++) {
      const layer = automation.layers[li];
      const rt = layerGraphs[li];
      if (!layer?.buffer || !rt) continue;
      const buf = wantStereo ? makeStereoBuffer(layer.buffer) : makeMonoBuffer(layer.buffer);
      const src = new AudioBufferSourceNode(offline, { buffer: buf });
      src.connect(rt.graph.input);
      const startAt = Math.max(0, layer.clipStartSec);
      src.start(startAt);
      src.stop(startAt + buf.duration);
    }

    const rendered = await offline.startRendering();
    const wav = encodeWavPcm16(rendered);
    const base = (automation.layers[automation.activeLayer]?.fileName || "automation").replace(/\.[^/.]+$/, "");
    downloadBytes(wav, `${base}-LAE-automation.wav`);
    setState("Exported");
  } catch (e) {
    console.error(e);
    setState("Export failed");
  } finally {
    els.exportBtn.disabled = false;
  }
}

function applyRealtimeSettings({ ramp = 0.02 } = {}) {
  if (viewMode === "automation") {
    applyAutomationGraphSettings({ ramp });
    return;
  }
  if (!realtime.ctx || !realtime.graph) return;
  const time = realtime.ctx.currentTime;
  realtime.graph.applyMaster(readMasterSettings(), { time, ramp });
  for (const m of modules) {
    const w = realtime.graph.modules.get(m.instanceId);
    if (!w) continue;
    w.setWetEnabled({ wet: m.wet ?? 1, enabled: Boolean(m.enabled) }, { time, ramp });
    w.applySettings(settingsForModule(m), { time, ramp });
  }
}

async function startPlayback() {
  if (viewMode === "automation") {
    await startAutomationPlayback();
    return;
  }
  if (!audioBuffer) return;
  await ensureRealtimeGraph();
  if (!realtime.ctx || !realtime.graph) return;
  stopPlayback();

  await realtime.ctx.resume();
  applyRealtimeSettings({ ramp: 0.03 });
  realtime.graph.resetAll();
  const baseTime = realtime.ctx.currentTime + 0.03;

  // Tape SFX can insert start/end sequences, so we may delay the main audio start.
  let audioStartAt = 0;
  let tapeEndStopAt = 0; // absolute context time
  const tape = modules.find((m) => m.type === "tape");
  if (tape?.enabled) {
    const p = tape.params || {};
    const sfxEnable = Boolean(p.sfxEnable);
    const bankId = String(p.sfxSource || "");
    const mode = String(p.sfxMode || "bed");
    const sfxLevel = Math.min(1, Math.max(0, Number(p.sfxLevel ?? 0.22)));
    const wantEdges = mode === "edges" || mode === "sequence";
    const wantBed = mode === "bed" || mode === "sequence";
    if (sfxEnable && bankId) {
      try {
        await ensureTapeBankDecoded(bankId);
        const assets = getTapeAssets(bankId);
        const w = realtime.graph.modules.get(tape.instanceId);
        if (assets && w) {
          const looping = Boolean(els.loopToggle.checked);
          const startDur = wantEdges ? tapeDurationOf(assets.start) : 0;
          const endDur = !looping && wantEdges ? tapeDurationOf(assets.end) : 0;
          audioStartAt = startDur;
          const audioStopAt = audioStartAt + audioBuffer.duration;
          tapeEndStopAt = !looping && wantEdges && endDur > 0 ? baseTime + audioStopAt + endDur : 0;

          for (const lg of w.laneGraphs || []) {
            const sfxNode = lg?.graph?.sfx;
            if (!sfxNode) continue;
            sfxNode.gain.setValueAtTime(sfxEnable ? sfxLevel : 0, realtime.ctx.currentTime);

            if (wantEdges && assets.start.length) {
              const r = tapeScheduleSequence(realtime.ctx, sfxNode, assets.start, baseTime);
              realtime.extraSources.push(...r.sources);
            }
            if (wantBed && assets.bed) {
              const bed = new AudioBufferSourceNode(realtime.ctx, { buffer: assets.bed });
              bed.loop = true;
              bed.connect(sfxNode);
              const off = tapePickBedOffset(audioDataSeed, assets.bed);
              bed.start(baseTime + audioStartAt, off);
              if (!looping) bed.stop(baseTime + audioStopAt);
              realtime.extraSources.push(bed);
            }
            if (!looping && wantEdges && assets.end.length) {
              const r = tapeScheduleSequence(realtime.ctx, sfxNode, assets.end, baseTime + audioStopAt);
              realtime.extraSources.push(...r.sources);
            }
          }
        }
      } catch (e) {
        console.warn(e);
      }
    }
  }

  // Television CRT bed (optional) is injected into the TV module's SFX bus.
  const tv = modules.find((m) => m.type === "television");
  realtime.playWindow = { baseTime, audioStartAt, tapeEndStopAt };
  if (tv?.enabled) {
    const p = tv.params || {};
    const bedEnable = Boolean(p.bedEnable);
    const bedSource = String(p.bedSource || "");
    if (bedEnable && bedSource) {
      try {
        const buf = await ensureTvBedDecoded(bedSource);
        const w = realtime.graph.modules.get(tv.instanceId);
        if (buf && w) {
          const looping = Boolean(els.loopToggle.checked);
          const endAt = tapeEndStopAt > 0 ? tapeEndStopAt : baseTime + audioStartAt + audioBuffer.duration;
          for (const lg of w.laneGraphs || []) {
            const sfxNode = lg?.graph?.nodes?.sfx || lg?.graph?.sfx || null;
            if (!sfxNode) continue;
            const bed = new AudioBufferSourceNode(realtime.ctx, { buffer: buf });
            bed.loop = true;
            bed.connect(sfxNode);
            bed.start(baseTime, 0);
            if (!looping) bed.stop(endAt);
            realtime.extraSources.push(bed);
            realtime.tvBedSources.push(bed);
          }
        }
      } catch (e) {
        console.warn(e);
      }
    }
  }

  // Camcorder SFX (wind/cam beds + gust hits) are injected via the module's wind bus.
  const camcorder = modules.find((m) => m.type === "camcorder");
  if (camcorder?.enabled) {
    const looping = Boolean(els.loopToggle.checked);
    const totalDur = looping ? audioBuffer.duration : tapeEndStopAt > 0 ? Math.max(0.05, tapeEndStopAt - baseTime) : audioStartAt + audioBuffer.duration;
    const camSettings = { ...settingsForModule(camcorder) };
    if (!camSettings.camBedSource && (camcorderSfxManifest.camBed || []).length) camSettings.camBedSource = camcorderSfxManifest.camBed[0];
    if (!camSettings.windBedSource && (camcorderSfxManifest.windBed || []).length) camSettings.windBedSource = camcorderSfxManifest.windBed[0];

    try {
      await ensureCamcorderSfxDecoded(camSettings);
      const w = realtime.graph.modules.get(camcorder.instanceId);
      if (w) {
        const stopAt = looping ? null : baseTime + totalDur;
        for (const lg of w.laneGraphs || []) {
          const windNode = lg?.graph?.wind || lg?.graph?.nodes?.wind || null;
          if (!windNode) continue;
          const seed = (lg?.seed ?? audioDataSeed) >>> 0;
          const created = scheduleCamcorderSfx(realtime.ctx, windNode, camSettings, seed, {
            startTime: baseTime,
            durationSeconds: totalDur,
            stopAt,
            looping,
          });
          realtime.extraSources.push(...created);
        }
      }
    } catch (e) {
      console.warn(e);
    }
  }

  const src = new AudioBufferSourceNode(realtime.ctx, { buffer: audioBuffer });
  src.loop = Boolean(els.loopToggle.checked);
  src.connect(realtime.graph.input);
  src.onended = () => {
    if (realtime.src === src) {
      realtime.src = null;
      realtime.playing = false;
      if (tapeEndStopAt > 0 && Number.isFinite(tapeEndStopAt) && realtime.ctx) {
        setState("Ejecting...");
        const ms = Math.max(0, Math.round((tapeEndStopAt - realtime.ctx.currentTime) * 1000));
        const tid = window.setTimeout(() => {
          if (realtime.src) return;
          setState(audioBuffer ? "Ready" : "Idle");
        }, ms);
        realtime.endTimers.push(tid);
      } else {
        setState("Stopped");
      }
      els.stopBtn.disabled = true;
    }
  };
  realtime.src = src;
  realtime.playing = true;
  src.start(baseTime + audioStartAt);
  setState("Playing");
  els.stopBtn.disabled = false;
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
  if (viewMode === "automation") {
    await exportAutomationWav();
    return;
  }
  if (!audioBuffer) return;
  els.exportBtn.disabled = true;
  setState("Rendering...");
  try {
    const wantStereo = !els.monoOut.checked;
    const ch = wantStereo ? 2 : 1;
    const sr = audioBuffer.sampleRate;

    // Tape SFX may prepend/append audio; account for it by extending the render length and delaying the main source.
    let tapeStartDur = 0;
    let tapeEndDur = 0;
    let tapeAssets = null;
    let tapeInst = null;
    let tapeMode = "bed";
    let tapeSfxLevel = 0.22;
    const tape = modules.find((m) => m.type === "tape");
    if (tape?.enabled) {
      const p = tape.params || {};
      const sfxEnable = Boolean(p.sfxEnable);
      const bankId = String(p.sfxSource || "");
      tapeMode = String(p.sfxMode || "bed");
      tapeSfxLevel = Math.min(1, Math.max(0, Number(p.sfxLevel ?? 0.22)));
      const wantEdges = tapeMode === "edges" || tapeMode === "sequence";
      if (sfxEnable && bankId && wantEdges) {
        try {
          await ensureTapeBankDecoded(bankId);
          tapeAssets = getTapeAssets(bankId);
          if (tapeAssets) {
            tapeStartDur = tapeDurationOf(tapeAssets.start);
            tapeEndDur = tapeDurationOf(tapeAssets.end);
            tapeInst = tape.instanceId;
          }
        } catch (e) {
          console.warn(e);
        }
      } else if (sfxEnable && bankId) {
        try {
          await ensureTapeBankDecoded(bankId);
          tapeAssets = getTapeAssets(bankId);
          tapeInst = tape.instanceId;
        } catch (e) {
          console.warn(e);
        }
      }
    }

    const totalDur = tapeStartDur + audioBuffer.duration + tapeEndDur;
    const frames = Math.max(1, Math.ceil(totalDur * sr));
    const offline = new OfflineAudioContext(ch, frames, sr);

    let sample = null;
    const trans = modules.find((m) => m.type === "transmission");
    if (trans?.enabled && trans.params?.tuningEnable && trans.params?.tuningSource && trans.params?.tuningSource !== "synth") {
      try {
        sample = await loadTuningSample(trans.params.tuningSource);
      } catch (e) {
        console.warn(e);
      }
    }

    const graph = await buildLameGraph(offline, {
      seed: audioDataSeed,
      modules: modules.filter((m) => Boolean(m.enabled)),
      stereo: wantStereo,
      tuningEdges,
      tuningSample: sample,
    });
    graph.output.connect(offline.destination);

    graph.applyMaster(readMasterSettings(), { time: 0, ramp: 0 });
    for (const m of modules) {
      const w = graph.modules.get(m.instanceId);
      if (!w) continue;
      w.setWetEnabled({ wet: m.wet ?? 1, enabled: Boolean(m.enabled) }, { time: 0, ramp: 0 });
      w.applySettings(settingsForModule(m), { time: 0, ramp: 0 });
    }
    graph.resetAll();

    // Schedule tape SFX into the tape module (pre-processor) so it matches preview behavior.
    if (tapeAssets && tapeInst) {
      const w = graph.modules.get(tapeInst);
      if (w) {
        const wantEdges = tapeMode === "edges" || tapeMode === "sequence";
        const wantBed = tapeMode === "bed" || tapeMode === "sequence";
        const audioStartAt = tapeStartDur;
        const audioStopAt = audioStartAt + audioBuffer.duration;
        const endAt = audioStopAt;
        for (const lg of w.laneGraphs || []) {
          const sfxNode = lg?.graph?.sfx;
          if (!sfxNode) continue;
          sfxNode.gain.setValueAtTime(tapeSfxLevel, 0);

          if (wantEdges && tapeAssets.start.length) tapeScheduleSequence(offline, sfxNode, tapeAssets.start, 0);
          if (wantBed && tapeAssets.bed) {
            const bed = new AudioBufferSourceNode(offline, { buffer: tapeAssets.bed });
            bed.loop = true;
            bed.connect(sfxNode);
            const off = tapePickBedOffset(audioDataSeed, tapeAssets.bed);
            bed.start(audioStartAt, off);
            bed.stop(audioStopAt);
          }
          if (wantEdges && tapeAssets.end.length) tapeScheduleSequence(offline, sfxNode, tapeAssets.end, endAt);
        }
      }
    }

    // Schedule TV CRT bed into the television module's SFX bus.
    const tv = modules.find((m) => m.type === "television");
    if (tv?.enabled) {
      const p = tv.params || {};
      const bedEnable = Boolean(p.bedEnable);
      const bedSource = String(p.bedSource || "");
      if (bedEnable && bedSource) {
        try {
          const buf = await ensureTvBedDecoded(bedSource);
          const w = graph.modules.get(tv.instanceId);
          if (buf && w) {
            for (const lg of w.laneGraphs || []) {
              const sfxNode = lg?.graph?.nodes?.sfx || lg?.graph?.sfx || null;
              if (!sfxNode) continue;
              const bed = new AudioBufferSourceNode(offline, { buffer: buf });
              bed.loop = true;
              bed.connect(sfxNode);
              bed.start(0, 0);
              bed.stop(totalDur);
            }
          }
        } catch (e) {
          console.warn(e);
        }
      }
    }

    // Schedule camcorder wind/cam SFX into the camcorder module's wind bus.
    const camcorder = modules.find((m) => m.type === "camcorder");
    if (camcorder?.enabled) {
      const w = graph.modules.get(camcorder.instanceId);
      if (w) {
        const camSettings = { ...settingsForModule(camcorder) };
        if (!camSettings.camBedSource && (camcorderSfxManifest.camBed || []).length) camSettings.camBedSource = camcorderSfxManifest.camBed[0];
        if (!camSettings.windBedSource && (camcorderSfxManifest.windBed || []).length) camSettings.windBedSource = camcorderSfxManifest.windBed[0];
        try {
          await ensureCamcorderSfxDecoded(camSettings);
          for (const lg of w.laneGraphs || []) {
            const windNode = lg?.graph?.wind || lg?.graph?.nodes?.wind || null;
            if (!windNode) continue;
            const seed = (lg?.seed ?? audioDataSeed) >>> 0;
            scheduleCamcorderSfx(offline, windNode, camSettings, seed, {
              startTime: 0,
              durationSeconds: totalDur,
              stopAt: totalDur,
              looping: false,
            });
          }
        } catch (e) {
          console.warn(e);
        }
      }
    }

    const src = new AudioBufferSourceNode(offline, { buffer: audioBuffer });
    src.connect(graph.input);
    src.start(tapeStartDur);
    const rendered = await offline.startRendering();

    const wav = encodeWavPcm16(rendered);
    const base = (els.fileName.textContent || "bounce").replace(/\.[^/.]+$/, "");
    downloadBytes(wav, `${base}-LAE.wav`);
    setState("Exported");
  } catch (e) {
    console.error(e);
    setState("Export failed");
  } finally {
    els.exportBtn.disabled = false;
  }
}

function ensureAutomationLayers() {
  if (automation.layers.length) return;
  for (let i = 0; i < MAX_LAYERS; i++) automation.layers.push(makeAutomationLayer(i));
}

async function decodeAudioFile(file) {
  const ab = await file.arrayBuffer();
  const seed = fnv1a32Sampled(new Uint8Array(ab));
  const decodeCtx = new (window.AudioContext || window.webkitAudioContext)();
  try {
    const decoded = await decodeCtx.decodeAudioData(ab.slice(0));
    return { decoded, seed };
  } finally {
    try {
      await decodeCtx.close();
    } catch {
      // ignore
    }
  }
}

function computePeaks(buffer, points = 2500) {
  const ch = buffer.numberOfChannels;
  const a = buffer.getChannelData(0);
  const b = ch > 1 ? buffer.getChannelData(1) : null;
  const n = buffer.length;
  const out = new Float32Array(Math.max(8, points | 0));
  for (let i = 0; i < out.length; i++) {
    const i0 = Math.floor((i / out.length) * n);
    const i1 = Math.floor(((i + 1) / out.length) * n);
    let m = 0;
    for (let j = i0; j < i1; j++) {
      const s = b ? 0.5 * (a[j] + b[j]) : a[j];
      const v = Math.abs(s);
      if (v > m) m = v;
    }
    out[i] = m;
  }
  return out;
}

function getAutomationLengthSec() {
  let end = 0;
  for (const layer of automation.layers) {
    if (!layer?.buffer) continue;
    end = Math.max(end, Math.max(0, layer.clipStartSec) + layer.buffer.duration);
  }
  return Math.max(6, end || 0);
}

function setActiveAutomationLayer(index) {
  const i = Math.max(0, Math.min(MAX_LAYERS - 1, index | 0));
  if (automation.activeLayer === i) return;
  automation.activeLayer = i;
  syncModuleRowColumns();
  renderModules();
  renderAutomationUi();
}

function makeAutomationTargetOptions() {
  const opts = [];
  for (let li = 0; li < automation.layers.length; li++) {
    const layer = automation.layers[li];
    opts.push({ value: `layer:${li}:enable`, text: `Layer ${li + 1} / Enable`, kind: "bool" });
    opts.push({ value: `layer:${li}:gain`, text: `Layer ${li + 1} / Gain`, kind: "float" });
    for (const m of layer.modules) {
      const label = m.name || m.type;
      opts.push({ value: `mod:${li}:${m.instanceId}:enable`, text: `Layer ${li + 1} / ${label} Enable`, kind: "bool" });
      opts.push({ value: `mod:${li}:${m.instanceId}:wet`, text: `Layer ${li + 1} / ${label} Wet`, kind: "float" });
    }
  }
  return opts;
}

function getLaneForTarget(target) {
  const t = String(target || "");
  const parts = t.split(":");
  if (parts[0] === "layer") {
    const li = Number(parts[1]);
    const key = String(parts[2] || "");
    const layer = automation.layers[li];
    if (!layer) return null;
    const existing = layer.automations.layer.get(key);
    if (existing) return existing;
    const lane = new Lane(key === "enable" ? "bool" : "float");
    layer.automations.layer.set(key, lane);
    return lane;
  }
  if (parts[0] === "mod") {
    const li = Number(parts[1]);
    const inst = Number(parts[2]);
    const key = String(parts[3] || "");
    const layer = automation.layers[li];
    if (!layer) return null;
    let entry = layer.automations.modules.get(inst);
    if (!entry) {
      entry = { wet: new Lane("float"), enabled: new Lane("bool") };
      layer.automations.modules.set(inst, entry);
    }
    return key === "enable" ? entry.enabled : entry.wet;
  }
  return null;
}

function drawAutomationLane() {
  if (!els.autoLaneCanvas) return;
  const canvas = els.autoLaneCanvas;
  const ctx = canvas.getContext("2d");
  if (!ctx) return;

  const dpr = Math.max(1, window.devicePixelRatio || 1);
  const len = getAutomationLengthSec();
  const w = Math.max(400, Math.ceil(len * automation.pxPerSec));
  const h = 140;
  canvas.width = Math.floor(w * dpr);
  canvas.height = Math.floor(h * dpr);
  canvas.style.width = `${w}px`;
  canvas.style.height = `${h}px`;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = "rgba(0,0,0,.10)";
  ctx.fillRect(0, 0, w, h);

  const bpm = Math.max(30, Math.min(240, Number(automation.bpm) || 120));
  const spb = 60 / bpm;
  ctx.lineWidth = 1;
  for (let s = 0; s <= len; s += 1) {
    const x = s * automation.pxPerSec;
    ctx.strokeStyle = s % 5 === 0 ? "rgba(233,237,242,.10)" : "rgba(233,237,242,.06)";
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, h);
    ctx.stroke();
  }
  for (let b = 0; b <= len; b += spb) {
    const x = b * automation.pxPerSec;
    ctx.strokeStyle = "rgba(255,180,84,.07)";
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, h);
    ctx.stroke();
  }

  const lane = getLaneForTarget(automation.target);
  if (!lane) return;
  const isBool = lane.kind === "bool";
  const toY = (v) => (isBool ? (v >= 0.5 ? 18 : h - 18) : (1 - clamp01(v)) * (h - 36) + 18);

  ctx.strokeStyle = "rgba(85,214,255,.85)";
  ctx.lineWidth = 2;
  ctx.beginPath();
  if (lane.points.length) {
    for (let i = 0; i < lane.points.length; i++) {
      const p = lane.points[i];
      const x = p.t * automation.pxPerSec;
      const y = toY(p.v);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
  } else {
    ctx.moveTo(0, toY(isBool ? 1 : 0));
    ctx.lineTo(w, toY(isBool ? 1 : 0));
  }
  ctx.stroke();

  for (const p of lane.points) {
    const x = p.t * automation.pxPerSec;
    const y = toY(p.v);
    ctx.fillStyle = "rgba(233,237,242,.92)";
    ctx.beginPath();
    ctx.arc(x, y, 4.5, 0, Math.PI * 2);
    ctx.fill();
  }
}

function drawLayerWaveform(layer, canvas) {
  const ctx = canvas.getContext("2d");
  if (!ctx) return;
  const dpr = Math.max(1, window.devicePixelRatio || 1);

  const len = getAutomationLengthSec();
  const w = Math.max(400, Math.ceil(len * automation.pxPerSec));
  const h = 86;
  canvas.width = Math.floor(w * dpr);
  canvas.height = Math.floor(h * dpr);
  canvas.style.width = `${w}px`;
  canvas.style.height = `${h}px`;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = "rgba(0,0,0,.10)";
  ctx.fillRect(0, 0, w, h);

  ctx.lineWidth = 1;
  for (let s = 0; s <= len; s += 1) {
    const x = s * automation.pxPerSec;
    ctx.strokeStyle = s % 5 === 0 ? "rgba(233,237,242,.08)" : "rgba(233,237,242,.04)";
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, h);
    ctx.stroke();
  }

  if (!layer.buffer || !layer.peaks) {
    ctx.fillStyle = "rgba(233,237,242,.5)";
    ctx.font = "12px " + getComputedStyle(document.body).fontFamily;
    ctx.fillText("Drop / load audio…", 10, h / 2 + 4);
    return;
  }

  const startX = layer.clipStartSec * automation.pxPerSec;
  const clipW = layer.buffer.duration * automation.pxPerSec;
  ctx.fillStyle = "rgba(85,214,255,.08)";
  ctx.fillRect(startX, 0, clipW, h);

  const peaks = layer.peaks;
  const mid = h / 2;
  const scaleX = clipW / peaks.length;
  ctx.strokeStyle = "rgba(233,237,242,.78)";
  ctx.lineWidth = 1;
  ctx.beginPath();
  for (let i = 0; i < peaks.length; i++) {
    const x = startX + i * scaleX;
    const amp = peaks[i];
    const y = amp * (h * 0.42);
    ctx.moveTo(x, mid - y);
    ctx.lineTo(x, mid + y);
  }
  ctx.stroke();

  ctx.strokeStyle = "rgba(255,180,84,.85)";
  ctx.lineWidth = 2;
  ctx.strokeRect(startX + 0.5, 0.5, Math.max(0, clipW - 1), h - 1);
}

function renderAutomationUi() {
  if (!els.automationView || els.automationView.classList.contains("hidden")) return;
  ensureAutomationLayers();

  const any = automation.layers.some((l) => l?.buffer);
  if (els.playBtn) els.playBtn.disabled = !any;
  if (els.exportBtn) els.exportBtn.disabled = !any;

  const bpm = Math.max(30, Math.min(240, Number(els.autoBpm?.value ?? automation.bpm) || 120));
  automation.bpm = bpm;
  const pxPerSec = Math.max(20, Math.min(400, Number(els.autoZoom?.value ?? automation.pxPerSec) || 100));
  automation.pxPerSec = pxPerSec;
  automation.snap = Boolean(els.autoSnap?.checked ?? automation.snap);

  if (els.autoBpmVal) els.autoBpmVal.textContent = String(bpm.toFixed(1));
  if (els.autoZoomVal) els.autoZoomVal.textContent = `${Math.round(pxPerSec)} px/s`;

  const len = getAutomationLengthSec();
  if (els.autoLenText) els.autoLenText.textContent = fmtTime(len);

  for (const layer of automation.layers) {
    const row = automation.ui.trackByIndex.get(layer.index);
    if (!row) continue;
    row.root.classList.toggle("active", automation.activeLayer === layer.index);
    row.fileName.textContent = layer.fileName || "Empty";
    row.startInput.value = String(Math.max(0, layer.clipStartSec).toFixed(3));
    drawLayerWaveform(layer, row.canvas);
  }

  drawAutomationLane();
}

function initAutomationUi() {
  ensureAutomationLayers();
  if (!els.automationTracks || !els.autoTarget || !els.autoLaneCanvas) return;

  els.automationTracks.replaceChildren();
  automation.ui.trackByIndex.clear();

  for (const layer of automation.layers) {
    const root = document.createElement("div");
    root.className = "track";

    const meta = document.createElement("div");
    meta.className = "trackMeta";

    const nameRow = document.createElement("div");
    nameRow.className = "row";
    const name = document.createElement("div");
    name.className = "name";
    name.textContent = layer.name;
    nameRow.appendChild(name);
    meta.appendChild(nameRow);

    const fileRow = document.createElement("div");
    fileRow.className = "row";
    const fileLabel = document.createElement("label");
    fileLabel.className = "file";
    const fileInput = document.createElement("input");
    fileInput.type = "file";
    fileInput.accept = ".wav,.mp3,audio/wav,audio/mpeg";
    const fileSpan = document.createElement("span");
    fileSpan.textContent = "Load";
    fileLabel.appendChild(fileInput);
    fileLabel.appendChild(fileSpan);
    fileRow.appendChild(fileLabel);

    const fileName = document.createElement("div");
    fileName.className = "hint";
    fileName.textContent = layer.fileName;
    fileRow.appendChild(fileName);
    meta.appendChild(fileRow);

    const startRow = document.createElement("div");
    startRow.className = "row";
    const startLab = document.createElement("div");
    startLab.className = "hint";
    startLab.textContent = "Start (s)";
    const startInput = document.createElement("input");
    startInput.type = "number";
    startInput.min = "0";
    startInput.step = "0.01";
    startInput.value = String(layer.clipStartSec);
    startRow.appendChild(startLab);
    startRow.appendChild(startInput);
    meta.appendChild(startRow);

    const canvasWrap = document.createElement("div");
    canvasWrap.className = "trackCanvasWrap";
    const canvas = document.createElement("canvas");
    canvas.height = 86;
    canvasWrap.appendChild(canvas);

    root.appendChild(meta);
    root.appendChild(canvasWrap);
    els.automationTracks.appendChild(root);

    name.addEventListener("click", () => setActiveAutomationLayer(layer.index));

    fileInput.addEventListener("change", async () => {
      const file = fileInput.files?.[0];
      if (!file) return;
      try {
        setState(`Decoding… (Layer ${layer.index + 1})`);
        const { decoded, seed } = await decodeAudioFile(file);
        layer.buffer = decoded;
        layer.seed = seed >>> 0;
        layer.tuningEdges = computeLeadingTrailingSilenceEdges(makeMonoBuffer(decoded), { thresholdDb: -50 });
        layer.peaks = computePeaks(decoded);
        layer.fileName = file.name;
        renderAutomationUi();
        setState("Ready");
      } catch (e) {
        console.error(e);
        setState(`Decode failed: ${e?.message || e}`);
      }
    });

    startInput.addEventListener("input", () => {
      layer.clipStartSec = Math.max(0, Number(startInput.value) || 0);
      renderAutomationUi();
    });

    let drag = null;
    const onMove = (ev) => {
      if (!drag) return;
      const rect = canvas.getBoundingClientRect();
      const x = ev.clientX - rect.left;
      const t = x / automation.pxPerSec;
      let next = Math.max(0, t - drag.offsetSec);
      if (automation.snap) {
        const spb = 60 / automation.bpm;
        next = Math.round(next / spb) * spb;
      }
      layer.clipStartSec = next;
      renderAutomationUi();
    };
    const onUp = () => {
      if (!drag) return;
      drag = null;
      window.removeEventListener("mousemove", onMove);
      window.removeEventListener("mouseup", onUp);
    };

    canvas.addEventListener("mousedown", (ev) => {
      if (!layer.buffer) return;
      const rect = canvas.getBoundingClientRect();
      const x = ev.clientX - rect.left;
      const t = x / automation.pxPerSec;
      const start = layer.clipStartSec;
      const end = start + layer.buffer.duration;
      if (t < start || t > end) return;
      setActiveAutomationLayer(layer.index);
      drag = { offsetSec: t - start };
      window.addEventListener("mousemove", onMove);
      window.addEventListener("mouseup", onUp);
    });

    automation.ui.trackByIndex.set(layer.index, { root, canvas, fileName, startInput });
  }

  const options = makeAutomationTargetOptions();
  els.autoTarget.replaceChildren();
  for (const o of options) {
    const opt = document.createElement("option");
    opt.value = o.value;
    opt.textContent = o.text;
    els.autoTarget.appendChild(opt);
  }
  automation.target = els.autoTarget.value || options[0]?.value || "";

  els.autoTarget.addEventListener("change", () => {
    automation.target = String(els.autoTarget.value || "");
    drawAutomationLane();
  });

  const updateAndDraw = () => renderAutomationUi();
  els.autoBpm?.addEventListener("input", updateAndDraw);
  els.autoZoom?.addEventListener("input", updateAndDraw);
  els.autoSnap?.addEventListener("change", updateAndDraw);

  els.autoLaneCanvas.addEventListener("contextmenu", (ev) => ev.preventDefault());
  els.autoLaneCanvas.addEventListener("mousedown", (ev) => {
    const lane = getLaneForTarget(automation.target);
    if (!lane) return;
    const rect = els.autoLaneCanvas.getBoundingClientRect();
    const x = ev.clientX - rect.left;
    const y = ev.clientY - rect.top;
    const t = Math.max(0, x / automation.pxPerSec);
    const v = 1 - (y - 18) / Math.max(1, rect.height - 36);

    if (ev.button === 2) {
      if (lane.removeNearest(t)) drawAutomationLane();
      return;
    }

    let best = -1;
    let bestDx = Infinity;
    for (let i = 0; i < lane.points.length; i++) {
      const px = lane.points[i].t * automation.pxPerSec;
      const dx = Math.abs(px - x);
      if (dx < bestDx) {
        bestDx = dx;
        best = i;
      }
    }
    if (best >= 0 && bestDx <= 10) {
      automation.ui.laneDragging = { lane, point: lane.points[best] };
      return;
    }

    lane.addPoint(t, lane.kind === "bool" ? (v >= 0.5 ? 1 : 0) : v);
    drawAutomationLane();
  });

  window.addEventListener("mousemove", (ev) => {
    const drag = automation.ui.laneDragging;
    if (!drag) return;
    const rect = els.autoLaneCanvas.getBoundingClientRect();
    const x = ev.clientX - rect.left;
    const y = ev.clientY - rect.top;
    const t = Math.max(0, x / automation.pxPerSec);
    const v = 1 - (y - 18) / Math.max(1, rect.height - 36);
    const p = drag.point;
    if (!p) return;
    p.t = t;
    p.v = drag.lane.kind === "bool" ? (v >= 0.5 ? 1 : 0) : clamp01(v);
    drag.lane.points.sort((a, b) => a.t - b.t);
    drawAutomationLane();
  });
  window.addEventListener("mouseup", () => {
    automation.ui.laneDragging = null;
  });

  // Keep lane scroll aligned with track scroll.
  const laneScrollEl = els.autoLaneCanvas.parentElement;
  if (laneScrollEl) {
    els.automationTracks.addEventListener("scroll", () => {
      try {
        laneScrollEl.scrollLeft = els.automationTracks.scrollLeft;
      } catch {
        // ignore
      }
    });
  }
}

async function loadFile(file) {
  if (!file) return;
  setState("Decoding...");
  await teardownRealtime();
  audioBuffer = null;
  sourceBuffer = null;
  tuningEdges = null;

  const ab = await file.arrayBuffer();
  audioDataSeed = fnv1a32Sampled(new Uint8Array(ab));

  const decodeCtx = new (window.AudioContext || window.webkitAudioContext)();
  try {
    sourceBuffer = await decodeCtx.decodeAudioData(ab.slice(0));
  } finally {
    try {
      await decodeCtx.close();
    } catch {
      // ignore
    }
  }

  audioBuffer = els.monoOut.checked ? makeMonoBuffer(sourceBuffer) : makeStereoBuffer(sourceBuffer);
  tuningEdges = computeLeadingTrailingSilenceEdges(makeMonoBuffer(sourceBuffer), { thresholdDb: -50 });

  refreshFileStatus(file.name);
  els.playBtn.disabled = false;
  els.exportBtn.disabled = false;
  setState("Ready");
}

els.fileInput.addEventListener("change", async () => {
  const file = els.fileInput.files?.[0];
  if (!file) return;
  try {
    if (viewMode === "automation") {
      ensureAutomationLayers();
      const layer = automation.layers[automation.activeLayer];
      if (!layer) return;
      setState(`Decoding… (Layer ${layer.index + 1})`);
      const { decoded, seed } = await decodeAudioFile(file);
      layer.buffer = decoded;
      layer.seed = seed >>> 0;
      layer.tuningEdges = computeLeadingTrailingSilenceEdges(makeMonoBuffer(decoded), { thresholdDb: -50 });
      layer.peaks = computePeaks(decoded);
      layer.fileName = file.name;
      renderAutomationUi();
      setState("Ready");
    } else {
      await loadFile(file);
    }
  } catch (e) {
    console.error(e);
    setState(`Load failed: ${e?.message || e}`);
  }
});

els.playBtn.addEventListener("click", async () => {
  try {
    await startPlayback();
  } catch (e) {
    console.error(e);
    setState(`Play failed: ${e?.message || e}`);
  }
});
els.stopBtn.addEventListener("click", () => {
  stopPlayback();
});
els.exportBtn.addEventListener("click", async () => {
  try {
    await exportWav();
  } catch (e) {
    console.error(e);
    setState(`Export failed: ${e?.message || e}`);
  }
});

els.loopToggle.addEventListener("change", () => {
  if (realtime.src) realtime.src.loop = Boolean(els.loopToggle.checked);
});

els.masterPreset?.addEventListener("change", async () => {
  const id = String(els.masterPreset.value || "");
  updateMasterPresetButtons();
  if (!id) return;
  try {
    await applyMasterPresetById(id);
  } catch (e) {
    console.warn(e);
  }
});

for (const el of [els.masterGain, els.masterHpHz, els.masterLpHz, els.masterComp, els.ceiling, els.limiter].filter(Boolean)) {
  el.addEventListener("input", () => {
    refreshMasterLabels();
    applyRealtimeSettings();
  });
}
els.softClip.addEventListener("change", () => {
  refreshMasterLabels();
  applyRealtimeSettings();
});
els.monoOut.addEventListener("change", async () => {
  refreshMasterLabels();
  if (sourceBuffer) {
    audioBuffer = els.monoOut.checked ? makeMonoBuffer(sourceBuffer) : makeStereoBuffer(sourceBuffer);
    refreshFileStatus(els.fileName.textContent);
    graphStale = true;
    await teardownRealtime();
    setState("Ready");
  }
});

els.saveMasterPresetBtn?.addEventListener("click", () => {
  const current = String(els.masterPreset?.value || "");
  const defaultName = current && current.startsWith("user:") ? current.slice("user:".length) : "my-preset";
  const name = window.prompt("Save suite preset name:", defaultName);
  if (!name) return;
  const id = slugifyId(name);
  const existing = getUserMasterPresets()[id];
  if (existing && !window.confirm(`Overwrite preset "${existing?.name || id}"?`)) return;

  const snapshot = {
    id,
    name: String(name),
    desc: "",
    master: { ...readMasterSettings(), monoOut: Boolean(els.monoOut.checked) },
    order: modules.map((m) => m.type),
    modules: Object.fromEntries(
      modules.map((m) => [
        m.type,
        { enabled: Boolean(m.enabled), wet: clamp01(m.wet ?? 1), params: { ...(m.params || {}) } },
      ]),
    ),
  };
  setUserMasterPreset(id, snapshot);
  refreshMasterPresetSelect({ keepValue: false });
  els.masterPreset.value = `user:${id}`;
  updateMasterPresetButtons();
});

els.deleteMasterPresetBtn?.addEventListener("click", () => {
  const current = String(els.masterPreset?.value || "");
  if (!current.startsWith("user:")) return;
  const id = current.slice("user:".length);
  const existing = getUserMasterPresets()[id];
  if (!window.confirm(`Delete suite preset "${existing?.name || id}"?`)) return;
  deleteUserMasterPreset(id);
  refreshMasterPresetSelect({ keepValue: false });
  updateMasterPresetButtons();
});

async function init() {
  refreshMasterLabels();
  tuningManifest = await loadTuningManifest();
  tapeSfxManifest = await loadTapeSfxManifest();
  camcorderSfxManifest = await loadCamcorderSfxManifest();
  tvSfxManifest = await loadTvSfxManifest();
  if (els.viewSelect) {
    const views = [
      { value: "suite", label: "Suite" },
      { value: "automation", label: "Automation" },
      { value: "occlusion", label: "Obfuscation Only" },
      { value: "transmission", label: "Transmission Only" },
      { value: "comms", label: "Comms Only" },
      { value: "conference", label: "Conference Only" },
      { value: "tape", label: "Tape Only" },
      { value: "television", label: "Television Only" },
      { value: "cartridge", label: "Cartridge Only" },
      { value: "cd", label: "CD Only" },
      { value: "camcorder", label: "Camcorder Only" },
    ];
    els.viewSelect.replaceChildren();
    for (const v of views) {
      const opt = document.createElement("option");
      opt.value = v.value;
      opt.textContent = v.label;
      els.viewSelect.appendChild(opt);
    }
    els.viewSelect.value = "suite";
    els.viewSelect.addEventListener("change", async () => {
      try {
        await applyViewMode(els.viewSelect.value);
      } catch (e) {
        console.error(e);
        setState(`View failed: ${e?.message || e}`);
      }
    });
  }
  if (els.masterPreset) {
    els.masterPreset.replaceChildren();
    const none = document.createElement("option");
    none.value = "";
    none.textContent = "Master presets…";
    els.masterPreset.appendChild(none);
    for (const p of MASTER_PRESETS) {
      const opt = document.createElement("option");
      opt.value = p.id;
      opt.textContent = p.name;
      opt.title = p.desc || "";
      els.masterPreset.appendChild(opt);
    }
    els.masterPreset.value = "";
  }
  refreshMasterPresetSelect({ keepValue: false });
  renderModules();
  initAutomationUi();
  refreshFileStatus("None");
  setState("Idle");
}

init();
