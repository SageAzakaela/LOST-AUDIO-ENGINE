import { buildTapeGraph, defaultSettings } from "./audio/graph.js?v=20260827.21";
import { encodeWavMono16 } from "./audio/wav.js";
import { PRESETS } from "./presets.js?v=20260827.21";

const MANIFEST_URL = new URL("../audio/manifest.json", import.meta.url);

const els = {
  fileInput: document.querySelector("#fileInput"),
  playBtn: document.querySelector("#playBtn"),
  stopBtn: document.querySelector("#stopBtn"),
  exportBtn: document.querySelector("#exportBtn"),
  loopToggle: document.querySelector("#loopToggle"),

  quality: document.querySelector("#quality"),
  age: document.querySelector("#age"),
  wow: document.querySelector("#wow"),
  glitch: document.querySelector("#glitch"),
  preset: document.querySelector("#preset"),
  sfxEnable: document.querySelector("#sfxEnable"),

  hpHz: document.querySelector("#hpHz"),
  lpHz: document.querySelector("#lpHz"),
  headBumpDb: document.querySelector("#headBumpDb"),
  headBumpHz: document.querySelector("#headBumpHz"),
  drive: document.querySelector("#drive"),
  comp: document.querySelector("#comp"),
  speed: document.querySelector("#speed"),
  wowDepthMs: document.querySelector("#wowDepthMs"),
  flutterDepthMs: document.querySelector("#flutterDepthMs"),
  hiss: document.querySelector("#hiss"),
  hum: document.querySelector("#hum"),
  dropout: document.querySelector("#dropout"),
  dropoutMs: document.querySelector("#dropoutMs"),
  ceiling: document.querySelector("#ceiling"),
  outGain: document.querySelector("#outGain"),

  sfxSource: document.querySelector("#sfxSource"),
  sfxLevel: document.querySelector("#sfxLevel"),
  sfxMode: document.querySelector("#sfxMode"),

  qualityVal: document.querySelector("#qualityVal"),
  ageVal: document.querySelector("#ageVal"),
  wowVal: document.querySelector("#wowVal"),
  glitchVal: document.querySelector("#glitchVal"),

  hpHzVal: document.querySelector("#hpHzVal"),
  lpHzVal: document.querySelector("#lpHzVal"),
  headBumpDbVal: document.querySelector("#headBumpDbVal"),
  headBumpHzVal: document.querySelector("#headBumpHzVal"),
  driveVal: document.querySelector("#driveVal"),
  compVal: document.querySelector("#compVal"),
  speedVal: document.querySelector("#speedVal"),
  wowDepthMsVal: document.querySelector("#wowDepthMsVal"),
  flutterDepthMsVal: document.querySelector("#flutterDepthMsVal"),
  hissVal: document.querySelector("#hissVal"),
  humVal: document.querySelector("#humVal"),
  dropoutVal: document.querySelector("#dropoutVal"),
  dropoutMsVal: document.querySelector("#dropoutMsVal"),
  ceilingVal: document.querySelector("#ceilingVal"),
  outGainVal: document.querySelector("#outGainVal"),
  sfxSourceVal: document.querySelector("#sfxSourceVal"),
  sfxLevelVal: document.querySelector("#sfxLevelVal"),
  sfxModeVal: document.querySelector("#sfxModeVal"),

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
let sfxBankId = "cassette";
let sfxBanks = [];
const sfxCache = new Map(); // filename -> AudioBuffer
let activeSfxSources = [];
const playback = {
  session: 0,
  endScheduled: false,
  endStopAt: 0,
};

let settings = defaultSettings();

function setState(text) {
  els.state.textContent = text;
}

function readPrimaryFromUI() {
  return {
    quality: parseFloat(els.quality.value),
    age: parseFloat(els.age.value),
    wow: parseFloat(els.wow.value),
    glitch: parseFloat(els.glitch.value),
  };
}

function computeMacroTargets(primary) {
  const quality = clamp01(primary.quality ?? 0.55);
  const age = clamp01(primary.age ?? 0.35);
  const wow = clamp01(primary.wow ?? 0.25);
  const glitch = clamp01(primary.glitch ?? 0.18);

  const q = Math.pow(1 - quality, 1.4);
  const a = Math.pow(age, 1.25);
  const w = Math.pow(wow, 1.3);
  const g = Math.pow(glitch, 1.35);

  const lpHz = Math.round(17500 - q * 14500); // 17.5k -> 3k
  const hpHz = Math.round(25 + q * 90); // 25 -> 115

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

function readSettingsFromUI() {
  return {
    quality: parseFloat(els.quality.value),
    age: parseFloat(els.age.value),
    wow: parseFloat(els.wow.value),
    glitch: parseFloat(els.glitch.value),
    sfxEnable: Boolean(els.sfxEnable.checked),

    hpHz: parseFloat(els.hpHz.value),
    lpHz: parseFloat(els.lpHz.value),
    headBumpDb: parseFloat(els.headBumpDb.value),
    headBumpHz: parseFloat(els.headBumpHz.value),
    drive: parseFloat(els.drive.value),
    comp: parseFloat(els.comp.value),
    speed: parseFloat(els.speed.value),
    wowDepthMs: parseFloat(els.wowDepthMs.value),
    flutterDepthMs: parseFloat(els.flutterDepthMs.value),
    hiss: parseFloat(els.hiss.value),
    hum: parseFloat(els.hum.value),
    dropout: parseFloat(els.dropout.value),
    dropoutMs: parseFloat(els.dropoutMs.value),
    ceiling: parseFloat(els.ceiling.value),
    outGain: parseFloat(els.outGain.value),

    sfxSource: els.sfxSource.value,
    sfxLevel: parseFloat(els.sfxLevel.value),
    sfxMode: els.sfxMode.value,
  };
}

function writeSettingsToUI(s) {
  els.quality.value = s.quality ?? 0.55;
  els.age.value = s.age ?? 0.35;
  els.wow.value = s.wow ?? 0.25;
  els.glitch.value = s.glitch ?? 0.18;
  els.sfxEnable.checked = Boolean(s.sfxEnable);

  els.hpHz.value = s.hpHz ?? 35;
  els.lpHz.value = s.lpHz ?? 11000;
  els.headBumpDb.value = s.headBumpDb ?? 2.2;
  els.headBumpHz.value = s.headBumpHz ?? 85;
  els.drive.value = s.drive ?? 0.35;
  els.comp.value = s.comp ?? 0.28;
  els.speed.value = s.speed ?? 1;
  els.wowDepthMs.value = s.wowDepthMs ?? 3.5;
  els.flutterDepthMs.value = s.flutterDepthMs ?? 1.2;
  els.hiss.value = s.hiss ?? 0.12;
  els.hum.value = s.hum ?? 0.05;
  els.dropout.value = s.dropout ?? 0.18;
  els.dropoutMs.value = s.dropoutMs ?? 38;
  els.ceiling.value = s.ceiling ?? 0.92;
  els.outGain.value = s.outGain ?? 0.98;

  if (typeof s.sfxSource === "string") els.sfxSource.value = s.sfxSource;
  els.sfxLevel.value = s.sfxLevel ?? 0.46;
  els.sfxMode.value = s.sfxMode ?? "bed";
  refreshValueLabels();
}

function refreshValueLabels() {
  const s = readSettingsFromUI();
  els.qualityVal.textContent = pct01(s.quality);
  els.ageVal.textContent = pct01(s.age);
  els.wowVal.textContent = pct01(s.wow);
  els.glitchVal.textContent = pct01(s.glitch);

  els.hpHzVal.textContent = fmtHz(s.hpHz);
  els.lpHzVal.textContent = fmtHz(s.lpHz);
  els.headBumpDbVal.textContent = `${Number(s.headBumpDb).toFixed(1)} dB`;
  els.headBumpHzVal.textContent = fmtHz(s.headBumpHz);
  els.driveVal.textContent = pct01(s.drive);
  els.compVal.textContent = pct01(s.comp);
  els.speedVal.textContent = `${Number(s.speed).toFixed(3)}x`;
  els.wowDepthMsVal.textContent = `${Number(s.wowDepthMs).toFixed(1)} ms`;
  els.flutterDepthMsVal.textContent = `${Number(s.flutterDepthMs).toFixed(2)} ms`;
  els.hissVal.textContent = pct01(s.hiss);
  els.humVal.textContent = pct01(s.hum);
  els.dropoutVal.textContent = pct01(s.dropout);
  els.dropoutMsVal.textContent = `${Math.round(s.dropoutMs)} ms`;
  els.ceilingVal.textContent = `${Math.round(s.ceiling * 100)}%`;
  els.outGainVal.textContent = `${s.outGain.toFixed(2)}x`;

  const bankName = sfxBanks.find((b) => b.id === s.sfxSource)?.name;
  els.sfxSourceVal.textContent = bankName || (s.sfxSource ? s.sfxSource : "None");
  els.sfxLevelVal.textContent = pct01(s.sfxLevel);
  els.sfxModeVal.textContent = s.sfxMode === "edges" ? "Edges" : s.sfxMode === "sequence" ? "Sequence" : "Bed";
}

function applyMacrosFromPrimaryToAdvanced() {
  const t = computeMacroTargets(readPrimaryFromUI());
  els.hpHz.value = t.hpHz;
  els.lpHz.value = t.lpHz;
  els.hiss.value = t.hiss;
  els.hum.value = t.hum;
  els.drive.value = t.drive;
  els.comp.value = t.comp;
  els.headBumpDb.value = t.headBumpDb;
  els.headBumpHz.value = t.headBumpHz;
  els.wowDepthMs.value = t.wowDepthMs;
  els.flutterDepthMs.value = t.flutterDepthMs;
  els.dropout.value = t.dropout;
  els.dropoutMs.value = t.dropoutMs;
  els.speed.value = t.speed;
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
  realtime.graph = await buildTapeGraph(realtime.ctx, { seed: audioDataSeed });
  realtime.graph.output.connect(realtime.ctx.destination);
  realtime.seed = audioDataSeed >>> 0;
  realtime.graph.applySettings(readSettingsFromUI(), { ramp: 0 });
}

function stopSfx() {
  for (const s of activeSfxSources) {
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
  activeSfxSources = [];
  if (realtime.graph && realtime.ctx) realtime.graph.sfx.gain.setValueAtTime(0, realtime.ctx.currentTime);
}

function scheduleSequence(ctx, connectNode, buffers, startTime, { gain = 1 } = {}) {
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

function pickBedOffset(seed, bedBuffer) {
  if (!bedBuffer) return 0;
  const span = Math.max(0.001, bedBuffer.duration - 0.02);
  const off = (seed >>> 0) % 997;
  return (off / 997) * span;
}

function getBankById(id) {
  return sfxBanks.find((b) => b.id === id) || null;
}

async function ensureBankDecoded(bankId) {
  const bank = getBankById(bankId);
  if (!bank) return null;
  const files = new Set();
  for (const list of [bank.bed, bank.start, bank.end]) {
    if (!Array.isArray(list)) continue;
    for (const f of list) if (typeof f === "string" && f) files.add(f);
  }
  const needed = Array.from(files).filter((f) => !sfxCache.has(f));
  if (needed.length === 0) return bank;

  const tmpCtx = new (window.AudioContext || window.webkitAudioContext)();
  try {
    for (const name of needed) {
      const url = new URL(`../audio/${name}`, import.meta.url);
      const buf = await decodeUrlToBuffer(tmpCtx, url.href);
      sfxCache.set(name, buf);
    }
  } finally {
    await tmpCtx.close();
  }
  return bank;
}

function getBankAssets(bankId) {
  const bank = getBankById(bankId);
  if (!bank) return null;
  const bedName = Array.isArray(bank.bed) ? bank.bed[0] : null;
  const bed = bedName ? sfxCache.get(bedName) : null;
  const start = Array.isArray(bank.start) ? bank.start.map((n) => sfxCache.get(n)).filter(Boolean) : [];
  const end = Array.isArray(bank.end) ? bank.end.map((n) => sfxCache.get(n)).filter(Boolean) : [];
  return { bed, start, end };
}

function durationOf(buffers) {
  let d = 0;
  for (const b of buffers) d += b?.duration ?? 0;
  return d;
}

function scheduleEndSequenceIfNeeded(ctx, gainNode, sessionId, settings, assets, endAt) {
  const endDur = durationOf(assets?.end ?? []);
  playback.endScheduled = false;
  playback.endStopAt = 0;
  if (!settings.sfxEnable || !settings.sfxSource) return;
  if (!(settings.sfxMode === "edges" || settings.sfxMode === "sequence")) return;
  if (!assets || endDur <= 0 || !Number.isFinite(endAt)) return;

  scheduleSequence(ctx, gainNode, assets.end, endAt);
  playback.endScheduled = true;
  playback.endStopAt = endAt + endDur;

  const ms = Math.max(0, Math.round((playback.endStopAt - ctx.currentTime) * 1000));
  window.setTimeout(() => {
    if (playback.session !== sessionId) return;
    if (realtime.src) return;
    setState("Ready");
  }, ms);
}

async function startPlayback() {
  if (!audioBuffer) return;
  await ensureRealtimeGraph();
  if (!realtime.ctx || !realtime.graph) return;

  stopPlayback();
  await realtime.ctx.resume();

  playback.session++;
  const sessionId = playback.session;

  const s = readSettingsFromUI();
  realtime.graph.applySettings(s, { ramp: 0.03 });
  realtime.graph.reset(realtime.seed);

  // Transport order: insert/play (start SFX) -> program audio -> stop/eject (end SFX).
  stopSfx();
  realtime.graph.sfx.gain.setValueAtTime(s.sfxEnable ? s.sfxLevel : 0, realtime.ctx.currentTime);

  const t0 = realtime.ctx.currentTime;
  const looping = Boolean(els.loopToggle.checked);
  playback.endScheduled = false;
  playback.endStopAt = 0;
  let assets = null;
  if (s.sfxEnable && s.sfxSource) {
    await ensureBankDecoded(s.sfxSource);
    assets = getBankAssets(s.sfxSource);
  }

  let startDur = 0;
  if (s.sfxEnable && assets && (s.sfxMode === "edges" || s.sfxMode === "sequence")) {
    const r = scheduleSequence(realtime.ctx, realtime.graph.sfx, assets.start, t0);
    activeSfxSources.push(...r.sources);
    startDur = r.duration;
  }

  const audioStartAt = t0 + startDur;
  const audioStopAt = looping ? Number.POSITIVE_INFINITY : audioStartAt + audioBuffer.duration;

  if (s.sfxEnable && assets && (s.sfxMode === "bed" || s.sfxMode === "sequence") && assets.bed) {
    const bed = new AudioBufferSourceNode(realtime.ctx, { buffer: assets.bed });
    bed.loop = true;
    bed.loopStart = 0;
    bed.loopEnd = assets.bed.duration;
    bed.connect(realtime.graph.sfx);
    const off = pickBedOffset(realtime.seed, assets.bed);
    bed.start(audioStartAt, off);
    if (!looping) bed.stop(audioStopAt);
    activeSfxSources.push(bed);
  }

  if (!looping) scheduleEndSequenceIfNeeded(realtime.ctx, realtime.graph.sfx, sessionId, s, assets, audioStopAt);

  const src = new AudioBufferSourceNode(realtime.ctx, { buffer: audioBuffer });
  src.loop = looping;
  src.loopStart = 0;
  src.loopEnd = audioBuffer.duration;
  src.connect(realtime.graph.input);
  src.onended = () => {
    if (realtime.src === src) {
      realtime.src = null;
      if (playback.endScheduled) setState("Ejecting...");
      else {
        stopSfx();
        setState("Stopped");
      }
    }
  };
  realtime.src = src;
  src.start(audioStartAt);
  setState(startDur > 0 ? "Inserting..." : "Playing");
  if (startDur > 0) {
    const ms = Math.max(0, Math.round((audioStartAt - realtime.ctx.currentTime) * 1000));
    window.setTimeout(() => {
      if (playback.session !== sessionId) return;
      if (!realtime.src) return;
      setState("Playing");
    }, ms);
  }
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
  stopSfx();
  setState(audioBuffer ? "Ready" : "Idle");
}

async function requestStop() {
  if (!realtime.ctx || !realtime.graph || !realtime.src) return stopPlayback();
  const s = readSettingsFromUI();
  const doEnd = s.sfxEnable && s.sfxSource && (s.sfxMode === "edges" || s.sfxMode === "sequence");

  playback.session++;
  const sessionId = playback.session;

  const now = realtime.ctx.currentTime;
  const stopAt = now + 0.03;

  try {
    realtime.src.stop(stopAt);
  } catch {
    // ignore
  }
  realtime.src = null;

  stopSfx();
  if (!doEnd) {
    setState("Stopped");
    return;
  }

  await ensureBankDecoded(s.sfxSource);
  const assets = getBankAssets(s.sfxSource);
  const endDur = durationOf(assets?.end ?? []);
  if (!assets || endDur <= 0) {
    setState("Stopped");
    return;
  }

  realtime.graph.sfx.gain.setValueAtTime(s.sfxLevel, now);
  scheduleSequence(realtime.ctx, realtime.graph.sfx, assets.end, stopAt);
  setState("Ejecting...");

  const ms = Math.max(0, Math.round((stopAt + endDur - now) * 1000));
  window.setTimeout(() => {
    if (playback.session !== sessionId) return;
    setState("Ready");
  }, ms);
}

async function exportWav() {
  if (!audioBuffer) return;
  els.exportBtn.disabled = true;
  setState("Rendering...");
  const s = readSettingsFromUI();
  let assets = null;
  if (s.sfxEnable && s.sfxSource) {
    await ensureBankDecoded(s.sfxSource);
    assets = getBankAssets(s.sfxSource);
  }

  const startDur = s.sfxEnable && assets && (s.sfxMode === "edges" || s.sfxMode === "sequence") ? durationOf(assets.start) : 0;
  const endDur = s.sfxEnable && assets && (s.sfxMode === "edges" || s.sfxMode === "sequence") ? durationOf(assets.end) : 0;
  const totalDur = startDur + audioBuffer.duration + endDur;
  const totalLen = Math.max(1, Math.ceil(totalDur * audioBuffer.sampleRate));

  const offline = new OfflineAudioContext({
    numberOfChannels: 1,
    length: totalLen,
    sampleRate: audioBuffer.sampleRate,
  });
  const graph = await buildTapeGraph(offline, { seed: audioDataSeed });
  graph.applySettings(s, { time: 0, ramp: 0 });
  graph.sfx.gain.setValueAtTime(s.sfxEnable ? s.sfxLevel : 0, 0);

  const audioStartAt = startDur;
  const audioStopAt = startDur + audioBuffer.duration;
  if (s.sfxEnable && assets) {
    if (s.sfxMode === "edges" || s.sfxMode === "sequence") scheduleSequence(offline, graph.sfx, assets.start, 0);
    if ((s.sfxMode === "bed" || s.sfxMode === "sequence") && assets.bed) {
      const bed = new AudioBufferSourceNode(offline, { buffer: assets.bed });
      bed.loop = true;
      bed.loopStart = 0;
      bed.loopEnd = assets.bed.duration;
      bed.connect(graph.sfx);
      const off = pickBedOffset(audioDataSeed, assets.bed);
      bed.start(audioStartAt, off);
      bed.stop(audioStopAt);
    }
    if (s.sfxMode === "edges" || s.sfxMode === "sequence") scheduleSequence(offline, graph.sfx, assets.end, audioStopAt);
  }
  const src = new AudioBufferSourceNode(offline, { buffer: audioBuffer });
  src.connect(graph.input);
  graph.output.connect(offline.destination);
  src.start(audioStartAt);
  const rendered = await offline.startRendering();
  const blob = encodeWavMono16(rendered);
  const url = URL.createObjectURL(blob);
  const base = (els.fileName.textContent || "export").replace(/\.[^/.]+$/, "");
  const a = document.createElement("a");
  a.href = url;
  a.download = `${base} - Tape Engine.wav`;
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
    realtime.graph.sfx.gain.setValueAtTime(settings.sfxEnable ? settings.sfxLevel : 0, realtime.ctx.currentTime);

    // If playing, keep SFX sensible when toggling/bank-switching: swap the bed immediately.
    if (realtime.src) {
      stopSfx();
      if (settings.sfxEnable && settings.sfxSource) {
        ensureBankDecoded(settings.sfxSource)
          .then(() => {
            const assets = getBankAssets(settings.sfxSource);
            if (!assets) return;
            if (settings.sfxMode === "bed" || settings.sfxMode === "sequence") {
              if (!assets.bed) return;
              const bed = new AudioBufferSourceNode(realtime.ctx, { buffer: assets.bed });
              bed.loop = true;
              bed.loopStart = 0;
              bed.loopEnd = assets.bed.duration;
              bed.connect(realtime.graph.sfx);
              const off = pickBedOffset(realtime.seed, assets.bed);
              bed.start(realtime.ctx.currentTime, off);
              activeSfxSources.push(bed);
            }
          })
          .catch(() => {});
      }
    }
  }
  els.preset.value = "";
}

function hookControls() {
  for (const el of [els.quality, els.age, els.wow, els.glitch]) {
    el.addEventListener("input", () => {
      applyMacrosFromPrimaryToAdvanced();
      applyToGraphAndMarkCustom();
    });
  }

  const adv = [
    els.sfxEnable,
    els.hpHz,
    els.lpHz,
    els.headBumpDb,
    els.headBumpHz,
    els.drive,
    els.comp,
    els.speed,
    els.wowDepthMs,
    els.flutterDepthMs,
    els.hiss,
    els.hum,
    els.dropout,
    els.dropoutMs,
    els.ceiling,
    els.outGain,
    els.sfxSource,
    els.sfxLevel,
    els.sfxMode,
  ];
  for (const el of adv) {
    const evt = el.tagName === "SELECT" || el.type === "checkbox" ? "change" : "input";
    el.addEventListener(evt, async () => {
      if (el === els.sfxSource) await loadSelectedSfx();
      applyToGraphAndMarkCustom();
    });
  }

  els.preset.addEventListener("change", async () => {
    const key = els.preset.value;
    if (!key) return;
    const preset = PRESETS[key];
    if (!preset) return;
    writeSettingsToUI({ ...defaultSettings(), ...preset });
    applyMacrosFromPrimaryToAdvanced();
    // Re-apply explicit preset overrides (macros may have changed these).
    for (const k of Object.keys(preset)) {
      if (k === "sfxEnable") els.sfxEnable.checked = Boolean(preset[k]);
      else if (k === "hpHz") els.hpHz.value = preset[k];
      else if (k === "lpHz") els.lpHz.value = preset[k];
      else if (k === "headBumpDb") els.headBumpDb.value = preset[k];
      else if (k === "headBumpHz") els.headBumpHz.value = preset[k];
      else if (k === "drive") els.drive.value = preset[k];
      else if (k === "comp") els.comp.value = preset[k];
      else if (k === "speed") els.speed.value = preset[k];
      else if (k === "wowDepthMs") els.wowDepthMs.value = preset[k];
      else if (k === "flutterDepthMs") els.flutterDepthMs.value = preset[k];
      else if (k === "hiss") els.hiss.value = preset[k];
      else if (k === "hum") els.hum.value = preset[k];
      else if (k === "dropout") els.dropout.value = preset[k];
      else if (k === "dropoutMs") els.dropoutMs.value = preset[k];
      else if (k === "ceiling") els.ceiling.value = preset[k];
      else if (k === "outGain") els.outGain.value = preset[k];
    }
    refreshValueLabels();
    if (realtime.graph && realtime.ctx) realtime.graph.applySettings(readSettingsFromUI(), { ramp: 0.03 });
  });
}

function wireButtons() {
  els.playBtn.addEventListener("click", () => startPlayback());
  els.stopBtn.addEventListener("click", () => requestStop());
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

async function loadTapeManifest() {
  try {
    const res = await fetch(MANIFEST_URL);
    if (!res.ok) return [];
    const json = await res.json();
    if (json && Array.isArray(json.banks)) {
      return json.banks
        .map((b) => ({
          id: typeof b.id === "string" ? b.id : "",
          name: typeof b.name === "string" ? b.name : "",
          bed: Array.isArray(b.bed) ? b.bed : [],
          start: Array.isArray(b.start) ? b.start : [],
          end: Array.isArray(b.end) ? b.end : [],
        }))
        .filter((b) => b.id);
    }
    // Fallback: simple sample list -> one bank.
    const list = Array.isArray(json) ? json : json && Array.isArray(json.samples) ? json.samples : [];
    const samples = list.filter((x) => typeof x === "string");
    if (samples.length === 0) return [];
    return [{ id: "custom", name: "Custom", bed: [samples[0]], start: [], end: [] }];
  } catch {
    return [];
  }
}

async function populateSfxSelect() {
  sfxBanks = await loadTapeManifest();
  els.sfxSource.innerHTML = "";
  const optNone = document.createElement("option");
  optNone.value = "";
  optNone.textContent = "(none)";
  els.sfxSource.appendChild(optNone);
  for (const bank of sfxBanks) {
    const opt = document.createElement("option");
    opt.value = bank.id;
    opt.textContent = bank.name || bank.id;
    els.sfxSource.appendChild(opt);
  }
  if (sfxBanks.length === 0) {
    optNone.textContent = "(no entries in tape-engine/audio/manifest.json)";
  }
  els.sfxSource.value = sfxBankId || "";
  refreshValueLabels();
}

async function loadSelectedSfx() {
  const bankId = els.sfxSource.value || "";
  sfxBankId = bankId;
  if (!bankId) return;
  try {
    await ensureBankDecoded(bankId);
  } catch (e) {
    console.warn(e);
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
populateSfxSelect();
setState("Idle");
