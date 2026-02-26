import { buildCamcorderGraph, defaultSettings } from "./audio/graph.js";
import { encodeWavMono16 } from "./audio/wav.js";
import { PRESETS } from "./presets.js";

const WIND_MANIFEST_URL = new URL("../audio/manifest.json", import.meta.url);

const els = {
  fileInput: document.querySelector("#fileInput"),
  playBtn: document.querySelector("#playBtn"),
  stopBtn: document.querySelector("#stopBtn"),
  exportBtn: document.querySelector("#exportBtn"),
  loopToggle: document.querySelector("#loopToggle"),

  coverage: document.querySelector("#coverage"),
  movement: document.querySelector("#movement"),
  corruption: document.querySelector("#corruption"),
  agc: document.querySelector("#agc"),
  preset: document.querySelector("#preset"),
  wind: document.querySelector("#wind"),
  windLevel: document.querySelector("#windLevel"),
  camLevel: document.querySelector("#camLevel"),
  windBedLevel: document.querySelector("#windBedLevel"),
  windHitLevel: document.querySelector("#windHitLevel"),
  windHitRate: document.querySelector("#windHitRate"),
  camBedSource: document.querySelector("#camBedSource"),
  windBedSource: document.querySelector("#windBedSource"),
  windHitSource: document.querySelector("#windHitSource"),

  hpHz: document.querySelector("#hpHz"),
  lpHz: document.querySelector("#lpHz"),
  boxDb: document.querySelector("#boxDb"),
  boxHz: document.querySelector("#boxHz"),
  agcAmt: document.querySelector("#agcAmt"),
  agcSpeed: document.querySelector("#agcSpeed"),
  clip: document.querySelector("#clip"),
  crush: document.querySelector("#crush"),
  bits: document.querySelector("#bits"),
  rate: document.querySelector("#rate"),
  drop: document.querySelector("#drop"),
  dropMs: document.querySelector("#dropMs"),
  dropMode: document.querySelector("#dropMode"),
  repeatMs: document.querySelector("#repeatMs"),
  chirp: document.querySelector("#chirp"),
  handling: document.querySelector("#handling"),
  rub: document.querySelector("#rub"),
  hiss: document.querySelector("#hiss"),
  ceiling: document.querySelector("#ceiling"),
  outGain: document.querySelector("#outGain"),

  coverageVal: document.querySelector("#coverageVal"),
  movementVal: document.querySelector("#movementVal"),
  corruptionVal: document.querySelector("#corruptionVal"),
  agcVal: document.querySelector("#agcVal"),
  windGroupVal: document.querySelector("#windGroupVal"),
  windLevelVal: document.querySelector("#windLevelVal"),
  camLevelVal: document.querySelector("#camLevelVal"),
  windBedLevelVal: document.querySelector("#windBedLevelVal"),
  windHitLevelVal: document.querySelector("#windHitLevelVal"),
  windHitRateVal: document.querySelector("#windHitRateVal"),
  camBedSourceVal: document.querySelector("#camBedSourceVal"),
  windBedSourceVal: document.querySelector("#windBedSourceVal"),
  windHitSourceVal: document.querySelector("#windHitSourceVal"),
  hpHzVal: document.querySelector("#hpHzVal"),
  lpHzVal: document.querySelector("#lpHzVal"),
  boxDbVal: document.querySelector("#boxDbVal"),
  boxHzVal: document.querySelector("#boxHzVal"),
  agcAmtVal: document.querySelector("#agcAmtVal"),
  agcSpeedVal: document.querySelector("#agcSpeedVal"),
  clipVal: document.querySelector("#clipVal"),
  crushVal: document.querySelector("#crushVal"),
  bitsVal: document.querySelector("#bitsVal"),
  rateVal: document.querySelector("#rateVal"),
  dropVal: document.querySelector("#dropVal"),
  dropMsVal: document.querySelector("#dropMsVal"),
  dropModeVal: document.querySelector("#dropModeVal"),
  repeatMsVal: document.querySelector("#repeatMsVal"),
  chirpVal: document.querySelector("#chirpVal"),
  handlingVal: document.querySelector("#handlingVal"),
  rubVal: document.querySelector("#rubVal"),
  hissVal: document.querySelector("#hissVal"),
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
const windCache = new Map(); // filename -> AudioBuffer
let windBedBuffer = null;
let windHitBuffer = null;
let activeWindSources = [];
let windLists = { camBed: [], windBed: [], windHits: [] };

let settings = defaultSettings();

function setState(text) {
  els.state.textContent = text;
}

function stopWind() {
  for (const s of activeWindSources) {
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
  activeWindSources = [];
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

async function decodeUrlToBuffer(ctx, url) {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`Failed to load ${url}`);
  const ab = await res.arrayBuffer();
  const decoded = await ctx.decodeAudioData(ab.slice(0));
  const mono = new AudioBuffer({ length: decoded.length, sampleRate: decoded.sampleRate, numberOfChannels: 1 });
  const out = mono.getChannelData(0);
  if (decoded.numberOfChannels === 1) out.set(decoded.getChannelData(0));
  else {
    const a = decoded.getChannelData(0);
    const b = decoded.getChannelData(1);
    for (let i = 0; i < out.length; i++) out[i] = 0.5 * (a[i] + b[i]);
  }
  return mono;
}

async function loadWindManifest() {
  try {
    const res = await fetch(WIND_MANIFEST_URL);
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

async function populateWindSelects() {
  windLists = await loadWindManifest();
  const populate = (select, list) => {
    const prev = select.value || "";
    select.innerHTML = "";
    const opt = document.createElement("option");
    opt.value = "";
    opt.textContent = list.length ? "(none)" : "(no entries in camcorder-engine/audio/manifest.json)";
    select.appendChild(opt);
    for (const name of list) {
      const o = document.createElement("option");
      o.value = name;
      o.textContent = name;
      select.appendChild(o);
    }
    select.value = prev;
  };
  populate(els.camBedSource, windLists.camBed);
  populate(els.windBedSource, windLists.windBed);
  populate(els.windHitSource, windLists.windHits);

  // If the user hasn't chosen anything yet, default to the first available sources.
  if (!els.camBedSource.value && windLists.camBed.length) els.camBedSource.value = windLists.camBed[0];
  if (!els.windBedSource.value && windLists.windBed.length) els.windBedSource.value = windLists.windBed[0];
}

async function loadWindBuffersIfNeeded() {
  const camName = els.camBedSource.value || "";
  const bedName = els.windBedSource.value || "";
  const hitName = els.windHitSource.value || "";

  windBedBuffer = null;
  windHitBuffer = null;

  const needed = [camName, bedName, hitName].filter((n) => n && !windCache.has(n));
  if (needed.length > 0) {
    const tmpCtx = new (window.AudioContext || window.webkitAudioContext)();
    try {
      for (const name of needed) {
        const url = new URL(`../audio/${encodeURIComponent(name)}`, import.meta.url);
        const buf = await decodeUrlToBuffer(tmpCtx, url.href);
        windCache.set(name, buf);
      }
    } finally {
      await tmpCtx.close();
    }
  }

  // Keep the cam bed in the cache; it shares the same bus and scheduling.
  if (bedName) windBedBuffer = windCache.get(bedName) || null;
  if (hitName) windHitBuffer = windCache.get(hitName) || null;
}

function startWindForContext(ctx, graph, seed, durationSeconds) {
  stopWind();
  if (!graph?.wind) return;
  const windEnabled = Boolean(els.wind.checked);
  const windLevel = parseFloat(els.windLevel.value);
  if (!windEnabled || windLevel <= 0.0001) return;

  const baseTime = ctx.currentTime;
  const camLevel = Math.max(0, Math.min(1, parseFloat(els.camLevel.value)));
  const windBedLevel = Math.max(0, Math.min(1, parseFloat(els.windBedLevel.value)));
  const windHitLevel = Math.max(0, Math.min(1, parseFloat(els.windHitLevel.value)));

  const camBuf = windCache.get(els.camBedSource.value || "") || null;
  if (camBuf && camLevel > 0.0001) {
    const g = new GainNode(ctx, { gain: camLevel });
    g.connect(graph.wind);
    const bed = new AudioBufferSourceNode(ctx, { buffer: camBuf });
    bed.loop = true;
    bed.loopStart = 0;
    bed.loopEnd = camBuf.duration;
    bed.connect(g);
    bed.start(baseTime, pickOffset(seed ^ 0x0ddc0ffe, camBuf));
    activeWindSources.push(bed);
  }

  if (windBedBuffer) {
    const g = new GainNode(ctx, { gain: windBedLevel });
    g.connect(graph.wind);
    const bed = new AudioBufferSourceNode(ctx, { buffer: windBedBuffer });
    bed.loop = true;
    bed.loopStart = 0;
    bed.loopEnd = windBedBuffer.duration;
    bed.connect(g);
    bed.start(baseTime, pickOffset(seed ^ 0x31415927, windBedBuffer));
    activeWindSources.push(bed);
  }

  if (windHitBuffer && windHitLevel > 0.0001 && durationSeconds > 0.05) {
    const g = new GainNode(ctx, { gain: windHitLevel });
    g.connect(graph.wind);
    const times = computeGustTimes(durationSeconds, seed ^ 0x27182818, parseFloat(els.windHitRate.value));
    const prng = new XorShift32((seed ^ 0x85ebca6b) >>> 0);
    for (const t of times) {
      const src = new AudioBufferSourceNode(ctx, { buffer: windHitBuffer });
      src.connect(g);
      const maxOff = Math.max(0, Math.min(0.08, windHitBuffer.duration - 0.02));
      const off = maxOff > 0 ? prng.nextFloat() * maxOff : 0;
      src.start(baseTime + t, off);
      activeWindSources.push(src);
    }
  }
}

function readPrimaryFromUI() {
  return {
    coverage: parseFloat(els.coverage.value),
    movement: parseFloat(els.movement.value),
    corruption: parseFloat(els.corruption.value),
    agc: parseFloat(els.agc.value),
  };
}

function computeMacroTargets(primary) {
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

function readSettingsFromUI() {
  return {
    coverage: parseFloat(els.coverage.value),
    movement: parseFloat(els.movement.value),
    corruption: parseFloat(els.corruption.value),
    agc: parseFloat(els.agc.value),
    wind: Boolean(els.wind.checked),
    windLevel: parseFloat(els.windLevel.value),
    camLevel: parseFloat(els.camLevel.value),
    windBedLevel: parseFloat(els.windBedLevel.value),
    windHitLevel: parseFloat(els.windHitLevel.value),
    windHitRate: parseFloat(els.windHitRate.value),
    camBedSource: els.camBedSource.value,
    windBedSource: els.windBedSource.value,
    windHitSource: els.windHitSource.value,

    hpHz: parseFloat(els.hpHz.value),
    lpHz: parseFloat(els.lpHz.value),
    boxDb: parseFloat(els.boxDb.value),
    boxHz: parseFloat(els.boxHz.value),
    agcAmt: parseFloat(els.agcAmt.value),
    agcSpeed: parseFloat(els.agcSpeed.value),
    clip: parseFloat(els.clip.value),
    crush: parseFloat(els.crush.value),
    bits: parseFloat(els.bits.value),
    rate: parseFloat(els.rate.value),
    drop: parseFloat(els.drop.value),
    dropMs: parseFloat(els.dropMs.value),
    dropMode: els.dropMode.value,
    repeatMs: parseFloat(els.repeatMs.value),
    chirp: parseFloat(els.chirp.value),
    handling: parseFloat(els.handling.value),
    rub: parseFloat(els.rub.value),
    hiss: parseFloat(els.hiss.value),
    ceiling: parseFloat(els.ceiling.value),
    outGain: parseFloat(els.outGain.value),
  };
}

function writeSettingsToUI(s) {
  els.coverage.value = s.coverage ?? 0.35;
  els.movement.value = s.movement ?? 0.25;
  els.corruption.value = s.corruption ?? 0.18;
  els.agc.value = s.agc ?? 0.35;
  els.wind.checked = Boolean(s.wind);
  els.windLevel.value = s.windLevel ?? 0.95;
  els.camLevel.value = s.camLevel ?? 0.35;
  els.windBedLevel.value = s.windBedLevel ?? 0.85;
  els.windHitLevel.value = s.windHitLevel ?? 0.65;
  els.windHitRate.value = s.windHitRate ?? 0.35;
  if (typeof s.camBedSource === "string") els.camBedSource.value = s.camBedSource;
  if (typeof s.windBedSource === "string") els.windBedSource.value = s.windBedSource;
  if (typeof s.windHitSource === "string") els.windHitSource.value = s.windHitSource;

  els.hpHz.value = s.hpHz ?? 55;
  els.lpHz.value = s.lpHz ?? 9200;
  els.boxDb.value = s.boxDb ?? 3.2;
  els.boxHz.value = s.boxHz ?? 1650;
  els.agcAmt.value = s.agcAmt ?? 0.55;
  els.agcSpeed.value = s.agcSpeed ?? 0.45;
  els.clip.value = s.clip ?? 0.25;
  els.crush.value = s.crush ?? 0.12;
  els.bits.value = s.bits ?? 12;
  els.rate.value = s.rate ?? 24000;
  els.drop.value = s.drop ?? 0.18;
  els.dropMs.value = s.dropMs ?? 28;
  els.dropMode.value = s.dropMode ?? "hold";
  els.repeatMs.value = s.repeatMs ?? 48;
  els.chirp.value = s.chirp ?? 0.15;
  els.handling.value = s.handling ?? 0.22;
  els.rub.value = s.rub ?? 0.18;
  els.hiss.value = s.hiss ?? 0.12;
  els.ceiling.value = s.ceiling ?? 0.92;
  els.outGain.value = s.outGain ?? 0.98;
  refreshValueLabels();
}

function refreshValueLabels() {
  const s = readSettingsFromUI();
  els.coverageVal.textContent = pct01(s.coverage);
  els.movementVal.textContent = pct01(s.movement);
  els.corruptionVal.textContent = pct01(s.corruption);
  els.agcVal.textContent = pct01(s.agc);
  if (els.windGroupVal) els.windGroupVal.textContent = s.wind ? "On" : "Off";
  els.windLevelVal.textContent = pct01(s.windLevel);
  els.camLevelVal.textContent = pct01(s.camLevel);
  els.windBedLevelVal.textContent = pct01(s.windBedLevel);
  els.windHitLevelVal.textContent = pct01(s.windHitLevel);
  els.windHitRateVal.textContent = pct01(s.windHitRate);
  els.camBedSourceVal.textContent = s.camBedSource ? s.camBedSource : "None";
  els.windBedSourceVal.textContent = s.windBedSource ? s.windBedSource : "None";
  els.windHitSourceVal.textContent = s.windHitSource ? s.windHitSource : "None";

  els.hpHzVal.textContent = fmtHz(s.hpHz);
  els.lpHzVal.textContent = fmtHz(s.lpHz);
  els.boxDbVal.textContent = `${Number(s.boxDb).toFixed(1)} dB`;
  els.boxHzVal.textContent = fmtHz(s.boxHz);
  els.agcAmtVal.textContent = pct01(s.agcAmt);
  els.agcSpeedVal.textContent = pct01(s.agcSpeed);
  els.clipVal.textContent = pct01(s.clip);
  els.crushVal.textContent = pct01(s.crush);
  els.bitsVal.textContent = `${Math.round(s.bits)}-bit`;
  els.rateVal.textContent = fmtHz(s.rate);
  els.dropVal.textContent = pct01(s.drop);
  els.dropMsVal.textContent = `${Math.round(s.dropMs)} ms`;
  els.dropModeVal.textContent = s.dropMode;
  els.repeatMsVal.textContent = `${Math.round(s.repeatMs)} ms`;
  els.chirpVal.textContent = pct01(s.chirp);
  els.handlingVal.textContent = pct01(s.handling);
  els.rubVal.textContent = pct01(s.rub);
  els.hissVal.textContent = pct01(s.hiss);
  els.ceilingVal.textContent = `${Math.round(s.ceiling * 100)}%`;
  els.outGainVal.textContent = `${s.outGain.toFixed(2)}x`;
}

function applyMacrosFromPrimaryToAdvanced() {
  const t = computeMacroTargets(readPrimaryFromUI());
  els.hpHz.value = t.hpHz;
  els.lpHz.value = t.lpHz;
  els.boxDb.value = t.boxDb;
  els.boxHz.value = t.boxHz;
  els.agcAmt.value = t.agcAmt;
  els.agcSpeed.value = t.agcSpeed;
  els.clip.value = t.clip;
  els.crush.value = t.crush;
  els.bits.value = t.bits;
  els.rate.value = t.rate;
  els.drop.value = t.drop;
  els.dropMs.value = t.dropMs;
  els.dropMode.value = t.dropMode;
  els.repeatMs.value = t.repeatMs;
  els.chirp.value = t.chirp;
  els.handling.value = t.handling;
  els.rub.value = t.rub;
  els.hiss.value = t.hiss;
  els.ceiling.value = t.ceiling;
  els.outGain.value = t.outGain;
  els.wind.checked = Boolean(t.wind);
  els.windLevel.value = t.windLevel;
  els.camLevel.value = t.camLevel;
  els.windBedLevel.value = t.windBedLevel;
  els.windHitLevel.value = t.windHitLevel;
  els.windHitRate.value = t.windHitRate;
  refreshValueLabels();
}

async function ensureRealtimeGraph() {
  if (realtime.ctx && realtime.graph) return;
  if (!audioBuffer) throw new Error("No audio loaded");
  realtime.ctx = new (window.AudioContext || window.webkitAudioContext)({
    numberOfChannels: 1,
    sampleRate: audioBuffer.sampleRate,
  });
  realtime.graph = await buildCamcorderGraph(realtime.ctx, { seed: audioDataSeed });
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
  await loadWindBuffersIfNeeded();
  startWindForContext(realtime.ctx, realtime.graph, realtime.seed, audioBuffer.duration);
  const src = new AudioBufferSourceNode(realtime.ctx, { buffer: audioBuffer });
  src.loop = Boolean(els.loopToggle.checked);
  src.loopStart = 0;
  src.loopEnd = audioBuffer.duration;
  src.connect(realtime.graph.input);
  src.onended = () => {
    if (realtime.src === src) {
      realtime.src = null;
      stopWind();
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
  stopWind();
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
  const graph = await buildCamcorderGraph(offline, { seed: audioDataSeed });
  graph.applySettings(s, { time: 0, ramp: 0 });

  await loadWindBuffersIfNeeded();
  if (s.wind && s.windLevel > 0.0001 && (windBedBuffer || windHitBuffer)) {
    const camBuf = windCache.get(s.camBedSource || "") || null;
    if (camBuf && (s.camLevel ?? 0) > 0.0001) {
      const g = new GainNode(offline, { gain: Math.max(0, Math.min(1, s.camLevel ?? 0.35)) });
      g.connect(graph.wind);
      const bed = new AudioBufferSourceNode(offline, { buffer: camBuf });
      bed.loop = true;
      bed.loopStart = 0;
      bed.loopEnd = camBuf.duration;
      bed.connect(g);
      bed.start(0, pickOffset(audioDataSeed ^ 0x0ddc0ffe, camBuf));
      bed.stop(audioBuffer.duration);
    }
    if (windBedBuffer) {
      const g = new GainNode(offline, { gain: Math.max(0, Math.min(1, s.windBedLevel ?? 0.85)) });
      g.connect(graph.wind);
      const bed = new AudioBufferSourceNode(offline, { buffer: windBedBuffer });
      bed.loop = true;
      bed.loopStart = 0;
      bed.loopEnd = windBedBuffer.duration;
      bed.connect(g);
      bed.start(0, pickOffset(audioDataSeed ^ 0x31415927, windBedBuffer));
      bed.stop(audioBuffer.duration);
    }
    if (windHitBuffer) {
      const times = computeGustTimes(audioBuffer.duration, audioDataSeed ^ 0x27182818, s.windHitRate ?? 0.35);
      const prng = new XorShift32((audioDataSeed ^ 0x85ebca6b) >>> 0);
      for (const t of times) {
        const srcW = new AudioBufferSourceNode(offline, { buffer: windHitBuffer });
        const g = new GainNode(offline, { gain: Math.max(0, Math.min(1, s.windHitLevel ?? 0.65)) });
        g.connect(graph.wind);
        srcW.connect(g);
        const maxOff = Math.max(0, Math.min(0.08, windHitBuffer.duration - 0.02));
        const off = maxOff > 0 ? prng.nextFloat() * maxOff : 0;
        srcW.start(t, off);
      }
    }
  }

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
  a.download = `${base} - Camcorder Engine.wav`;
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
  if (realtime.graph && realtime.ctx) {
    realtime.graph.applySettings(settings, { ramp: 0.03 });
    if (realtime.src) {
      loadWindBuffersIfNeeded()
        .then(() => startWindForContext(realtime.ctx, realtime.graph, realtime.seed, audioBuffer?.duration ?? 0))
        .catch(() => {});
    }
  }
  els.preset.value = "";
}

function hookControls() {
  for (const el of [els.coverage, els.movement, els.corruption, els.agc]) {
    el.addEventListener("input", () => {
      applyMacrosFromPrimaryToAdvanced();
      applyToGraphAndMarkCustom();
    });
  }

  for (const el of [
    els.wind,
    els.windLevel,
    els.camLevel,
    els.windBedLevel,
    els.windHitLevel,
    els.windHitRate,
    els.camBedSource,
    els.windBedSource,
    els.windHitSource,
    els.hpHz,
    els.lpHz,
    els.boxDb,
    els.boxHz,
    els.agcAmt,
    els.agcSpeed,
    els.clip,
    els.crush,
    els.bits,
    els.rate,
    els.drop,
    els.dropMs,
    els.dropMode,
    els.repeatMs,
    els.chirp,
    els.handling,
    els.rub,
    els.hiss,
    els.ceiling,
    els.outGain,
  ]) {
    const evt = el.tagName === "SELECT" || el.type === "checkbox" ? "change" : "input";
    el.addEventListener(evt, async () => {
      if (el === els.camBedSource || el === els.windBedSource || el === els.windHitSource) await loadWindBuffersIfNeeded();
      applyToGraphAndMarkCustom();
    });
  }

  els.preset.addEventListener("change", () => {
    const key = els.preset.value;
    if (!key) return;
    const preset = PRESETS[key];
    if (!preset) return;
    writeSettingsToUI({ ...defaultSettings(), ...preset });
    applyMacrosFromPrimaryToAdvanced();
    // Re-apply explicit advanced overrides from preset.
    const s = PRESETS[key];
    if ("wind" in s) els.wind.checked = Boolean(s.wind);
    if ("windLevel" in s) els.windLevel.value = s.windLevel;
    if ("camLevel" in s) els.camLevel.value = s.camLevel;
    if ("windBedLevel" in s) els.windBedLevel.value = s.windBedLevel;
    if ("windHitLevel" in s) els.windHitLevel.value = s.windHitLevel;
    if ("windHitRate" in s) els.windHitRate.value = s.windHitRate;
    if ("camBedSource" in s) els.camBedSource.value = s.camBedSource;
    if ("windBedSource" in s) els.windBedSource.value = s.windBedSource;
    if ("windHitSource" in s) els.windHitSource.value = s.windHitSource;
    if ("hpHz" in s) els.hpHz.value = s.hpHz;
    if ("lpHz" in s) els.lpHz.value = s.lpHz;
    if ("boxDb" in s) els.boxDb.value = s.boxDb;
    if ("boxHz" in s) els.boxHz.value = s.boxHz;
    if ("agcAmt" in s) els.agcAmt.value = s.agcAmt;
    if ("agcSpeed" in s) els.agcSpeed.value = s.agcSpeed;
    if ("clip" in s) els.clip.value = s.clip;
    if ("crush" in s) els.crush.value = s.crush;
    if ("bits" in s) els.bits.value = s.bits;
    if ("rate" in s) els.rate.value = s.rate;
    if ("drop" in s) els.drop.value = s.drop;
    if ("dropMs" in s) els.dropMs.value = s.dropMs;
    if ("dropMode" in s) els.dropMode.value = s.dropMode;
    if ("repeatMs" in s) els.repeatMs.value = s.repeatMs;
    if ("chirp" in s) els.chirp.value = s.chirp;
    if ("handling" in s) els.handling.value = s.handling;
    if ("rub" in s) els.rub.value = s.rub;
    if ("hiss" in s) els.hiss.value = s.hiss;
    if ("ceiling" in s) els.ceiling.value = s.ceiling;
    if ("outGain" in s) els.outGain.value = s.outGain;
    refreshValueLabels();
    if (realtime.graph && realtime.ctx) realtime.graph.applySettings(readSettingsFromUI(), { ramp: 0.03 });
    loadWindBuffersIfNeeded()
      .then(() => {
        if (realtime.src) startWindForContext(realtime.ctx, realtime.graph, realtime.seed, audioBuffer?.duration ?? 0);
      })
      .catch(() => {});
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
populateWindSelects()
  .catch(() => {})
  .finally(() => {
    writeSettingsToUI(settings);
    applyMacrosFromPrimaryToAdvanced();
    setState("Idle");
  });
