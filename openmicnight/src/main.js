import { buildOpenMicGraph, defaultSettings } from "./audio/graph.js";
import { encodeWavPcm16 } from "./audio/wav.js";

const PAGE_BASE_URL = new URL("./", window.location.href);
const MANIFEST_URL = new URL("audio/manifest.json", PAGE_BASE_URL).href;

const els = {
  fileInput: document.querySelector("#fileInput"),
  playBtn: document.querySelector("#playBtn"),
  stopBtn: document.querySelector("#stopBtn"),
  exportBtn: document.querySelector("#exportBtn"),
  loopToggle: document.querySelector("#loopToggle"),

  clipSelect: document.querySelector("#clipSelect"),
  crowdSelect: document.querySelector("#crowdSelect"),
  crowdLevel: document.querySelector("#crowdLevel"),

  sequenceToggle: document.querySelector("#sequenceToggle"),
  sequenceVal: document.querySelector("#sequenceVal"),
  xfadeMs: document.querySelector("#xfadeMs"),
  xfadeMsVal: document.querySelector("#xfadeMsVal"),

  introSelect: document.querySelector("#introSelect"),
  introFile: document.querySelector("#introFile"),
  introVal: document.querySelector("#introVal"),
  introFileVal: document.querySelector("#introFileVal"),

  outroSelect: document.querySelector("#outroSelect"),
  outroFile: document.querySelector("#outroFile"),
  outroVal: document.querySelector("#outroVal"),
  outroFileVal: document.querySelector("#outroFileVal"),

  cheerToggle: document.querySelector("#cheerToggle"),
  cheerSelect: document.querySelector("#cheerSelect"),
  cheerWhen: document.querySelector("#cheerWhen"),
  cheerLevel: document.querySelector("#cheerLevel"),
  cheerVal: document.querySelector("#cheerVal"),
  cheerLevelVal: document.querySelector("#cheerLevelVal"),

  bed1Select: document.querySelector("#bed1Select"),
  bed1Where: document.querySelector("#bed1Where"),
  bed1Level: document.querySelector("#bed1Level"),
  bed1Random: document.querySelector("#bed1Random"),
  bed1Val: document.querySelector("#bed1Val"),
  bed1LevelVal: document.querySelector("#bed1LevelVal"),

  bed2Select: document.querySelector("#bed2Select"),
  bed2Where: document.querySelector("#bed2Where"),
  bed2Level: document.querySelector("#bed2Level"),
  bed2Random: document.querySelector("#bed2Random"),
  bed2Val: document.querySelector("#bed2Val"),
  bed2LevelVal: document.querySelector("#bed2LevelVal"),

  hotMic: document.querySelector("#hotMic"),
  fbFreq: document.querySelector("#fbFreq"),
  ringQ: document.querySelector("#ringQ"),
  wall: document.querySelector("#wall"),
  room: document.querySelector("#room"),
  outGain: document.querySelector("#outGain"),

  fbDelayMs: document.querySelector("#fbDelayMs"),
  fbTone: document.querySelector("#fbTone"),
  limit: document.querySelector("#limit"),

  clipVal: document.querySelector("#clipVal"),
  crowdVal: document.querySelector("#crowdVal"),
  crowdLevelVal: document.querySelector("#crowdLevelVal"),
  hotMicVal: document.querySelector("#hotMicVal"),
  fbFreqVal: document.querySelector("#fbFreqVal"),
  ringQVal: document.querySelector("#ringQVal"),
  wallVal: document.querySelector("#wallVal"),
  roomVal: document.querySelector("#roomVal"),
  outGainVal: document.querySelector("#outGainVal"),
  fbDelayMsVal: document.querySelector("#fbDelayMsVal"),
  fbToneVal: document.querySelector("#fbToneVal"),
  limitVal: document.querySelector("#limitVal"),

  sourceName: document.querySelector("#sourceName"),
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

function clamp(x, lo, hi) {
  return Math.min(hi, Math.max(lo, x));
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

const realtime = { ctx: null, graph: null, sources: [], mainSrc: null, seed: 0, playing: false };

let audioBuffer = null; // main
let mainName = "";
let audioDataSeed = 0; // main seed

let introBuffer = null;
let introName = "";
let introSeed = 0;

let outroBuffer = null;
let outroName = "";
let outroSeed = 0;
let manifest = { micCheck: [], bandCheck: [], crowd: [] };
const clipCache = new Map(); // name -> AudioBuffer (mono)

let settings = defaultSettings();

function setState(text) {
  els.state.textContent = text;
}

function readSettingsFromUI() {
  return {
    crowdLevel: clamp01(Number(els.crowdLevel.value)),
    hotMic: clamp01(Number(els.hotMic.value)),
    fbFreq: Number(els.fbFreq.value),
    ringQ: Number(els.ringQ.value),
    wall: clamp01(Number(els.wall.value)),
    room: clamp01(Number(els.room.value)),
    outGain: Number(els.outGain.value),
    fbDelayMs: Number(els.fbDelayMs.value),
    fbTone: clamp01(Number(els.fbTone.value)),
    limit: clamp01(Number(els.limit.value)),
  };
}

function isSequenceEnabled() {
  return Boolean(els.sequenceToggle?.checked);
}

function readSequenceFromUI() {
  return {
    enabled: isSequenceEnabled(),
    xfadeSec: clamp(Number(els.xfadeMs?.value ?? 0) / 1000, 0, 2.0),
    cheer: {
      enabled: Boolean(els.cheerToggle?.checked),
      name: els.cheerSelect?.value || "",
      when: els.cheerWhen?.value || "songEnd",
      level: clamp01(Number(els.cheerLevel?.value ?? 0.6)),
    },
    beds: [
      {
        slot: 0,
        name: els.crowdSelect?.value || "",
        where: "all",
        level: 1,
        random: true,
      },
      {
        slot: 1,
        name: els.bed1Select?.value || "",
        where: els.bed1Where?.value || "all",
        level: clamp01(Number(els.bed1Level?.value ?? 0)),
        random: Boolean(els.bed1Random?.checked),
      },
      {
        slot: 2,
        name: els.bed2Select?.value || "",
        where: els.bed2Where?.value || "all",
        level: clamp01(Number(els.bed2Level?.value ?? 0)),
        random: Boolean(els.bed2Random?.checked),
      },
    ],
  };
}

function refreshValueLabels() {
  const s = readSettingsFromUI();
  const seq = readSequenceFromUI();
  els.clipVal.textContent = els.clipSelect.value ? els.clipSelect.value : "(none)";
  els.crowdVal.textContent = els.crowdSelect.value ? els.crowdSelect.value : "(none)";
  els.crowdLevelVal.textContent = pct01(s.crowdLevel);
  els.hotMicVal.textContent = pct01(s.hotMic);
  els.fbFreqVal.textContent = fmtHz(s.fbFreq);
  els.ringQVal.textContent = `${Number(s.ringQ).toFixed(1)} Q`;
  els.wallVal.textContent = pct01(s.wall);
  els.roomVal.textContent = pct01(s.room);
  els.outGainVal.textContent = `${Number(s.outGain).toFixed(2)}x`;
  els.fbDelayMsVal.textContent = `${Number(s.fbDelayMs).toFixed(1)} ms`;
  els.fbToneVal.textContent = pct01(s.fbTone);
  els.limitVal.textContent = pct01(s.limit);

  if (els.sequenceVal) els.sequenceVal.textContent = seq.enabled ? "On" : "Off";
  if (els.xfadeMsVal) els.xfadeMsVal.textContent = `${Math.round(seq.xfadeSec * 1000)} ms`;

  if (els.introVal) els.introVal.textContent = introName || "(none)";
  if (els.outroVal) els.outroVal.textContent = outroName || "(none)";
  if (els.introFileVal) els.introFileVal.textContent = els.introFile?.files?.[0]?.name || "";
  if (els.outroFileVal) els.outroFileVal.textContent = els.outroFile?.files?.[0]?.name || "";

  if (els.cheerVal) els.cheerVal.textContent = seq.cheer.enabled ? (seq.cheer.name || "(none)") : "Off";
  if (els.cheerLevelVal) els.cheerLevelVal.textContent = pct01(seq.cheer.level);

  if (els.bed1Val) els.bed1Val.textContent = els.bed1Select.value ? els.bed1Select.value : "(none)";
  if (els.bed2Val) els.bed2Val.textContent = els.bed2Select.value ? els.bed2Select.value : "(none)";
  if (els.bed1LevelVal) els.bed1LevelVal.textContent = pct01(Number(els.bed1Level.value));
  if (els.bed2LevelVal) els.bed2LevelVal.textContent = pct01(Number(els.bed2Level.value));
}

function pickDefaultClip() {
  const all = [...(manifest.micCheck || []), ...(manifest.bandCheck || [])];
  return all[0] || "";
}

function setSelectOptions(select, items, { noneLabel = "(none)" } = {}) {
  const prev = select.value || "";
  select.replaceChildren();
  const none = document.createElement("option");
  none.value = "";
  none.textContent = noneLabel;
  select.appendChild(none);
  for (const name of items) {
    const opt = document.createElement("option");
    opt.value = name;
    opt.textContent = name;
    select.appendChild(opt);
  }
  select.value = prev;
}

async function loadManifest() {
  try {
    const res = await fetch(MANIFEST_URL);
    if (!res.ok) return { micCheck: [], bandCheck: [], crowd: [] };
    const json = await res.json();
    const list = (x) => (Array.isArray(x) ? x.filter((s) => typeof s === "string") : []);
    return { micCheck: list(json.micCheck), bandCheck: list(json.bandCheck), crowd: list(json.crowd) };
  } catch {
    return { micCheck: [], bandCheck: [], crowd: [] };
  }
}

function getDecodeCtx() {
  return new (window.AudioContext || window.webkitAudioContext)();
}

async function decodeUrlToMonoBuffer(url) {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`Failed to load ${url}`);
  const ab = await res.arrayBuffer();
  const ctx = getDecodeCtx();
  try {
    const decoded = await ctx.decodeAudioData(ab.slice(0));
    const mono = ctx.createBuffer(1, decoded.length, decoded.sampleRate);
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
      await ctx.close();
    } catch {
      // ignore
    }
  }
}

function clipUrlFromName(name) {
  return new URL(`audio/${encodeURIComponent(name)}`, PAGE_BASE_URL).href;
}

async function ensureClipCached(name) {
  if (!name) return null;
  if (clipCache.has(name)) return clipCache.get(name);
  const buf = await decodeUrlToMonoBuffer(clipUrlFromName(name));
  clipCache.set(name, buf);
  return buf;
}

function mixSeed(a, b) {
  const x = (a ^ b) >>> 0;
  return (Math.imul(x, 2654435761) >>> 0) || 1;
}

function currentSeed() {
  let s = audioDataSeed >>> 0;
  if (!s) s = 1;
  if (!isSequenceEnabled()) return s >>> 0;
  if (introBuffer) s = mixSeed(s, introSeed >>> 0);
  if (outroBuffer) s = mixSeed(s, outroSeed >>> 0);
  return s >>> 0;
}

function computeSequenceTimeline({ xfadeSec, introDur, mainDur, outroDur }) {
  const xf = clamp(xfadeSec, 0, 2.0);
  const hasIntro = introDur > 0;
  const hasOutro = outroDur > 0;
  const introStart = 0;
  const mainStart = hasIntro ? Math.max(0, introDur - xf) : 0;
  const outroStart = hasOutro ? Math.max(mainStart, mainStart + mainDur - xf) : mainStart + mainDur;
  const totalDur = hasOutro ? outroStart + outroDur : mainStart + mainDur;
  return { xf, hasIntro, hasOutro, introStart, mainStart, outroStart, totalDur };
}

function refreshStatus() {
  const seq = readSequenceFromUI();
  const main = audioBuffer;
  if (!main) {
    els.sourceName.textContent = "None";
    els.duration.textContent = "-";
    els.sampleRate.textContent = "-";
    els.playBtn.disabled = true;
    els.exportBtn.disabled = true;
    setState("Idle");
    return;
  }

  const sr = main.sampleRate || 44100;
  let dur = main.duration;
  let label = mainName || "Main";
  if (seq.enabled) {
    const tl = computeSequenceTimeline({
      xfadeSec: seq.xfadeSec,
      introDur: introBuffer ? introBuffer.duration : 0,
      mainDur: main.duration,
      outroDur: outroBuffer ? outroBuffer.duration : 0,
    });
    dur = tl.totalDur;
    const parts = [];
    if (introBuffer) parts.push("intro");
    parts.push("main");
    if (outroBuffer) parts.push("outro");
    label = `Sequence (${parts.join(" + ")})`;
  }

  els.sourceName.textContent = label;
  els.duration.textContent = fmtTime(dur);
  els.sampleRate.textContent = `${sr} Hz`;
  els.playBtn.disabled = false;
  els.exportBtn.disabled = false;
  setState("Ready");
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

function stopPlayback() {
  for (const s of realtime.sources) {
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
  realtime.sources = [];
  realtime.mainSrc = null;
  realtime.playing = false;
  els.stopBtn.disabled = true;
  setState(audioBuffer ? "Ready" : "Idle");
}

async function ensureRealtimeGraph() {
  if (!audioBuffer) return;
  if (realtime.ctx && realtime.graph) return;
  const Ctx = window.AudioContext || window.webkitAudioContext;
  realtime.ctx = new Ctx({ latencyHint: "interactive" });
  realtime.graph = await buildOpenMicGraph(realtime.ctx, { seed: currentSeed() });
  realtime.graph.output.connect(realtime.ctx.destination);
  realtime.seed = currentSeed();
  realtime.graph.reset(realtime.seed);
  realtime.graph.applySettings(readSettingsFromUI(), { ramp: 0 });
}

function applyRealtimeSettings() {
  if (!realtime.ctx || !realtime.graph) return;
  realtime.graph.applySettings(readSettingsFromUI(), { ramp: 0.03 });
}

function computeSeedForBuffer(buf, fallbackName = "") {
  try {
    const ch = buf.getChannelData(0);
    const n = Math.min(ch.length, 16384);
    const bytes = new Uint8Array(n);
    for (let i = 0; i < n; i++) {
      const v = Math.max(-1, Math.min(1, ch[i]));
      bytes[i] = Math.floor((v * 0.5 + 0.5) * 255) & 255;
    }
    return fnv1a32Sampled(bytes);
  } catch {
    const enc = new TextEncoder();
    return fnv1a32Sampled(enc.encode(fallbackName || "openmicnight"));
  }
}

async function startPlayback() {
  if (!audioBuffer) return;
  await ensureRealtimeGraph();
  stopPlayback();
  await realtime.ctx.resume();
  const seed = currentSeed();
  realtime.seed = seed;
  realtime.graph.reset(seed);
  realtime.graph.applySettings(readSettingsFromUI(), { ramp: 0 });

  const t0 = realtime.ctx.currentTime + 0.02;
  const seq = readSequenceFromUI();

  const makeRng = (seed0) => {
    let s = (seed0 >>> 0) || 1;
    return () => {
      s ^= s << 13;
      s ^= s >>> 17;
      s ^= s << 5;
      return ((s >>> 0) / 0xffffffff) || 0;
    };
  };

  const scheduleBed = async (bedBuf, bedCfg, spanStart, spanEnd, slotSeed, { stopAtEnd = true } = {}) => {
    if (!bedBuf) return;
    if (!(spanEnd > spanStart + 0.02)) return;
    const rng = makeRng(slotSeed);
    const offset = bedCfg.random ? rng() * Math.max(0.001, bedBuf.duration) : 0;
    const g = new GainNode(realtime.ctx, { gain: bedCfg.level });
    const src = new AudioBufferSourceNode(realtime.ctx, { buffer: bedBuf });
    src.loop = true;
    src.connect(g);
    g.connect(realtime.graph.crowd);
    realtime.sources.push(src);
    src.start(spanStart, offset);
    if (stopAtEnd) src.stop(spanEnd);
  };

  const scheduleSeg = (buf, startAbs, stopAbs, gainEnv) => {
    if (!buf) return null;
    const g = new GainNode(realtime.ctx, { gain: 1 });
    const src = new AudioBufferSourceNode(realtime.ctx, { buffer: buf });
    src.connect(g);
    g.connect(realtime.graph.input);
    if (gainEnv?.length) {
      g.gain.cancelScheduledValues(0);
      for (const pt of gainEnv) g.gain.setValueAtTime(pt.v, pt.t);
    }
    realtime.sources.push(src);
    src.start(startAbs);
    if (stopAbs != null) src.stop(stopAbs);
    return src;
  };

  if (!seq.enabled) {
    const src = new AudioBufferSourceNode(realtime.ctx, { buffer: audioBuffer });
    src.loop = Boolean(els.loopToggle.checked);
    src.connect(realtime.graph.input);
    realtime.sources.push(src);
    realtime.mainSrc = src;
    src.start(t0);

    const crowdName = els.crowdSelect.value || "";
    const crowdBuf = crowdName ? await ensureClipCached(crowdName) : null;
    if (crowdBuf) {
      const crowdSrc = new AudioBufferSourceNode(realtime.ctx, { buffer: crowdBuf });
      crowdSrc.loop = true;
      crowdSrc.connect(realtime.graph.crowd);
      realtime.sources.push(crowdSrc);
      crowdSrc.start(t0);
    }
  } else {
    const tl = computeSequenceTimeline({
      xfadeSec: seq.xfadeSec,
      introDur: introBuffer ? introBuffer.duration : 0,
      mainDur: audioBuffer.duration,
      outroDur: outroBuffer ? outroBuffer.duration : 0,
    });
    const xf = tl.xf;

    const introAbs = t0 + tl.introStart;
    const mainAbs = t0 + tl.mainStart;
    const outroAbs = t0 + tl.outroStart;
    const endAbs = t0 + tl.totalDur;

    const loopMain = Boolean(els.loopToggle.checked);

    if (introBuffer) {
      const env = xf > 0 ? [{ t: introAbs, v: 1 }, { t: mainAbs, v: 1 }, { t: mainAbs + xf, v: 0 }] : [{ t: introAbs, v: 1 }];
      scheduleSeg(introBuffer, introAbs, introAbs + introBuffer.duration + 0.02, env);
    }

    const mainGain = new GainNode(realtime.ctx, { gain: 1 });
    const mainSrc = new AudioBufferSourceNode(realtime.ctx, { buffer: audioBuffer });
    mainSrc.loop = loopMain;
    mainSrc.connect(mainGain);
    mainGain.connect(realtime.graph.input);
    realtime.sources.push(mainSrc);
    realtime.mainSrc = mainSrc;

    mainGain.gain.cancelScheduledValues(0);
    if (introBuffer && xf > 0) {
      mainGain.gain.setValueAtTime(0, mainAbs);
      mainGain.gain.linearRampToValueAtTime(1, mainAbs + xf);
    } else {
      mainGain.gain.setValueAtTime(1, mainAbs);
    }
    if (outroBuffer && xf > 0 && !loopMain) {
      mainGain.gain.setValueAtTime(1, outroAbs);
      mainGain.gain.linearRampToValueAtTime(0, outroAbs + xf);
    }

    mainSrc.start(mainAbs);
    if (!loopMain) mainSrc.stop(mainAbs + audioBuffer.duration + 0.02);

    if (outroBuffer && !loopMain) {
      const env = xf > 0 ? [{ t: outroAbs, v: 0 }, { t: outroAbs + xf, v: 1 }] : [{ t: outroAbs, v: 1 }];
      scheduleSeg(outroBuffer, outroAbs, outroAbs + outroBuffer.duration + 0.02, env);
    }

    const regions = {
      intro: introBuffer ? [introAbs, mainAbs + xf] : null,
      main: [mainAbs, loopMain ? null : outroAbs + (outroBuffer ? xf : 0)],
      outro: outroBuffer && !loopMain ? [outroAbs, endAbs] : null,
      all: [introAbs, loopMain ? null : endAbs],
    };

    for (const bedCfg of seq.beds || []) {
      const name = bedCfg.name || "";
      if (!name) continue;
      const buf = await ensureClipCached(name);
      if (!buf) continue;
      const slotSeed = mixSeed(seed, (bedCfg.slot + 1) * 0x9e3779b1);
      const r = regions[bedCfg.where] || regions.all;
      if (!r) continue;
      const [rs, re] = r;
      if (re == null) await scheduleBed(buf, bedCfg, rs, rs + 3600, slotSeed, { stopAtEnd: false });
      else await scheduleBed(buf, bedCfg, rs, re, slotSeed, { stopAtEnd: true });
    }

    if (seq.cheer.enabled && !loopMain) {
      const name = seq.cheer.name || "";
      const buf = name ? await ensureClipCached(name) : null;
      if (buf) {
        const cheerAbs = seq.cheer.when === "outroStart" ? outroAbs : mainAbs + audioBuffer.duration - 0.05;
        const g = new GainNode(realtime.ctx, { gain: seq.cheer.level });
        const sfx = new AudioBufferSourceNode(realtime.ctx, { buffer: buf });
        sfx.connect(g);
        g.connect(realtime.graph.input);
        realtime.sources.push(sfx);
        sfx.start(Math.max(t0, cheerAbs));
      }
    }
  }

  realtime.playing = true;
  setState(seq.enabled && els.loopToggle.checked ? "Playing (Loop Main)" : "Playing");
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
  if (!audioBuffer) return;
  els.exportBtn.disabled = true;
  setState("Rendering...");
  try {
    const seq = readSequenceFromUI();
    const seed = currentSeed();
    const sr = audioBuffer.sampleRate || 44100;

    let totalDur = audioBuffer.duration;
    if (seq.enabled) {
      totalDur = computeSequenceTimeline({
        xfadeSec: seq.xfadeSec,
        introDur: introBuffer ? introBuffer.duration : 0,
        mainDur: audioBuffer.duration,
        outroDur: outroBuffer ? outroBuffer.duration : 0,
      }).totalDur;
    }

    const frames = Math.max(1, Math.ceil(totalDur * sr));
    const offline = new OfflineAudioContext(1, frames, sr);
    const graph = await buildOpenMicGraph(offline, { seed });
    graph.output.connect(offline.destination);
    graph.applySettings(readSettingsFromUI(), { time: 0, ramp: 0 });
    graph.reset(seed);

    const scheduleSegOffline = (buf, start, gainEnv) => {
      if (!buf) return;
      const g = new GainNode(offline, { gain: 1 });
      const src = new AudioBufferSourceNode(offline, { buffer: buf });
      src.connect(g);
      g.connect(graph.input);
      if (gainEnv?.length) {
        g.gain.cancelScheduledValues(0);
        for (const pt of gainEnv) g.gain.setValueAtTime(pt.v, pt.t);
      }
      src.start(start);
      src.stop(start + buf.duration + 0.02);
    };

    const scheduleBedOffline = (buf, bedCfg, spanStart, spanEnd, slotSeed) => {
      if (!buf) return;
      if (!(spanEnd > spanStart + 0.02)) return;
      let s = (slotSeed >>> 0) || 1;
      const rnd = () => {
        s ^= s << 13;
        s ^= s >>> 17;
        s ^= s << 5;
        return ((s >>> 0) / 0xffffffff) || 0;
      };
      const offset = bedCfg.random ? rnd() * Math.max(0.001, buf.duration) : 0;
      const g = new GainNode(offline, { gain: bedCfg.level });
      const src = new AudioBufferSourceNode(offline, { buffer: buf });
      src.loop = true;
      src.connect(g);
      g.connect(graph.crowd);
      src.start(spanStart, offset);
      src.stop(spanEnd);
    };

    if (!seq.enabled) {
      const src = new AudioBufferSourceNode(offline, { buffer: audioBuffer });
      src.connect(graph.input);
      src.start(0);

      const crowdName = els.crowdSelect.value || "";
      const crowdBuf = crowdName ? await ensureClipCached(crowdName) : null;
      if (crowdBuf) {
        const crowdSrc = new AudioBufferSourceNode(offline, { buffer: crowdBuf });
        crowdSrc.loop = true;
        crowdSrc.connect(graph.crowd);
        crowdSrc.start(0);
      }
    } else {
      const tl = computeSequenceTimeline({
        xfadeSec: seq.xfadeSec,
        introDur: introBuffer ? introBuffer.duration : 0,
        mainDur: audioBuffer.duration,
        outroDur: outroBuffer ? outroBuffer.duration : 0,
      });
      const xf = tl.xf;
      const introAbs = tl.introStart;
      const mainAbs = tl.mainStart;
      const outroAbs = tl.outroStart;
      const endAbs = tl.totalDur;

      if (introBuffer) {
        const env = xf > 0 ? [{ t: introAbs, v: 1 }, { t: mainAbs, v: 1 }, { t: mainAbs + xf, v: 0 }] : [{ t: introAbs, v: 1 }];
        scheduleSegOffline(introBuffer, introAbs, env);
      }

      const mainEnv = [];
      if (introBuffer && xf > 0) mainEnv.push({ t: mainAbs, v: 0 }, { t: mainAbs + xf, v: 1 });
      else mainEnv.push({ t: mainAbs, v: 1 });
      if (outroBuffer && xf > 0) mainEnv.push({ t: outroAbs, v: 1 }, { t: outroAbs + xf, v: 0 });
      scheduleSegOffline(audioBuffer, mainAbs, mainEnv);

      if (outroBuffer) {
        const env = xf > 0 ? [{ t: outroAbs, v: 0 }, { t: outroAbs + xf, v: 1 }] : [{ t: outroAbs, v: 1 }];
        scheduleSegOffline(outroBuffer, outroAbs, env);
      }

      const regions = {
        intro: introBuffer ? [introAbs, mainAbs + xf] : null,
        main: [mainAbs, outroAbs + (outroBuffer ? xf : 0)],
        outro: outroBuffer ? [outroAbs, endAbs] : null,
        all: [0, endAbs],
      };

      for (const bedCfg of seq.beds || []) {
        const name = bedCfg.name || "";
        if (!name) continue;
        const buf = await ensureClipCached(name);
        if (!buf) continue;
        const slotSeed = mixSeed(seed, (bedCfg.slot + 1) * 0x9e3779b1);
        const r = regions[bedCfg.where] || regions.all;
        if (!r) continue;
        scheduleBedOffline(buf, bedCfg, r[0], r[1], slotSeed);
      }

      if (seq.cheer.enabled) {
        const name = seq.cheer.name || "";
        const buf = name ? await ensureClipCached(name) : null;
        if (buf) {
          const cheerAbs = seq.cheer.when === "outroStart" ? outroAbs : mainAbs + audioBuffer.duration - 0.05;
          const g = new GainNode(offline, { gain: seq.cheer.level });
          const sfx = new AudioBufferSourceNode(offline, { buffer: buf });
          sfx.connect(g);
          g.connect(graph.input);
          sfx.start(Math.max(0, cheerAbs));
          sfx.stop(Math.min(endAbs + 0.25, cheerAbs + buf.duration + 0.02));
        }
      }
    }

    const rendered = await offline.startRendering();
    const wav = encodeWavPcm16(rendered);
    const base = (els.sourceName.textContent || "openmicnight").replace(/\.[^/.]+$/, "");
    downloadBytes(wav, `${base}-openmicnight.wav`);
    setState("Exported");
  } catch (e) {
    console.error(e);
    setState("Export failed");
  } finally {
    els.exportBtn.disabled = false;
  }
}

async function handleFileUpload(file) {
  if (!file) return;
  setState("Decoding...");
  stopPlayback();
  const ab = await file.arrayBuffer();
  const ctx = getDecodeCtx();
  try {
    const decoded = await ctx.decodeAudioData(ab.slice(0));
    const mono = ctx.createBuffer(1, decoded.length, decoded.sampleRate);
    const out = mono.getChannelData(0);
    if (decoded.numberOfChannels === 1) out.set(decoded.getChannelData(0));
    else {
      const a = decoded.getChannelData(0);
      const b = decoded.getChannelData(1);
      for (let i = 0; i < out.length; i++) out[i] = 0.5 * (a[i] + b[i]);
    }
    audioDataSeed = fnv1a32Sampled(new Uint8Array(ab));
    audioBuffer = mono;
    mainName = file.name || "Main Upload";
    refreshStatus();
  } finally {
    try {
      await ctx.close();
    } catch {
      // ignore
    }
  }
}

async function handleBuiltInClip(name) {
  if (!name) return;
  setState("Loading clip...");
  stopPlayback();
  const buf = await ensureClipCached(name);
  audioDataSeed = computeSeedForBuffer(buf, name);
  audioBuffer = buf;
  mainName = name || "Main Clip";
  refreshStatus();
}

async function decodeFileToMono(file) {
  const ab = await file.arrayBuffer();
  const ctx = getDecodeCtx();
  try {
    const decoded = await ctx.decodeAudioData(ab.slice(0));
    const mono = ctx.createBuffer(1, decoded.length, decoded.sampleRate);
    const out = mono.getChannelData(0);
    if (decoded.numberOfChannels === 1) out.set(decoded.getChannelData(0));
    else {
      const a = decoded.getChannelData(0);
      const b = decoded.getChannelData(1);
      for (let i = 0; i < out.length; i++) out[i] = 0.5 * (a[i] + b[i]);
    }
    return { buffer: mono, bytes: new Uint8Array(ab) };
  } finally {
    try {
      await ctx.close();
    } catch {
      // ignore
    }
  }
}

async function setIntroFromBuiltIn(name) {
  stopPlayback();
  if (!name) {
    introBuffer = null;
    introName = "";
    introSeed = 0;
    refreshStatus();
    return;
  }
  const buf = await ensureClipCached(name);
  introBuffer = buf;
  introName = name;
  introSeed = computeSeedForBuffer(buf, name);
  if (els.introFile) els.introFile.value = "";
  refreshStatus();
}

async function setOutroFromBuiltIn(name) {
  stopPlayback();
  if (!name) {
    outroBuffer = null;
    outroName = "";
    outroSeed = 0;
    refreshStatus();
    return;
  }
  const buf = await ensureClipCached(name);
  outroBuffer = buf;
  outroName = name;
  outroSeed = computeSeedForBuffer(buf, name);
  if (els.outroFile) els.outroFile.value = "";
  refreshStatus();
}

async function setIntroFromFile(file) {
  if (!file) return;
  setState("Decoding intro...");
  stopPlayback();
  const { buffer, bytes } = await decodeFileToMono(file);
  introBuffer = buffer;
  introName = file.name || "Intro Upload";
  introSeed = fnv1a32Sampled(bytes);
  if (els.introSelect) els.introSelect.value = "";
  refreshStatus();
}

async function setOutroFromFile(file) {
  if (!file) return;
  setState("Decoding outro...");
  stopPlayback();
  const { buffer, bytes } = await decodeFileToMono(file);
  outroBuffer = buffer;
  outroName = file.name || "Outro Upload";
  outroSeed = fnv1a32Sampled(bytes);
  if (els.outroSelect) els.outroSelect.value = "";
  refreshStatus();
}

function wireEvents() {
  els.fileInput.addEventListener("change", async () => {
    const file = els.fileInput.files?.[0];
    if (!file) return;
    els.clipSelect.value = "";
    await handleFileUpload(file);
    refreshValueLabels();
  });

  els.clipSelect.addEventListener("change", async () => {
    const name = els.clipSelect.value || "";
    if (!name) return;
    els.fileInput.value = "";
    await handleBuiltInClip(name);
    refreshValueLabels();
  });

  els.crowdSelect.addEventListener("change", async () => {
    refreshValueLabels();
    if (realtime.playing) {
      await startPlayback();
    }
  });

  els.loopToggle.addEventListener("change", () => {
    if (isSequenceEnabled()) {
      if (realtime.playing) startPlayback();
      return;
    }
    if (realtime.mainSrc) realtime.mainSrc.loop = Boolean(els.loopToggle.checked);
  });

  els.playBtn.addEventListener("click", async () => {
    await startPlayback();
  });
  els.stopBtn.addEventListener("click", () => stopPlayback());
  els.exportBtn.addEventListener("click", async () => exportWav());

  if (els.sequenceToggle) {
    els.sequenceToggle.addEventListener("change", () => {
      refreshValueLabels();
      refreshStatus();
      if (realtime.playing) startPlayback();
    });
  }
  if (els.xfadeMs) {
    els.xfadeMs.addEventListener("input", () => {
      refreshValueLabels();
      refreshStatus();
      if (realtime.playing && isSequenceEnabled()) startPlayback();
    });
  }

  if (els.introSelect) {
    els.introSelect.addEventListener("change", async () => {
      await setIntroFromBuiltIn(els.introSelect.value || "");
      refreshValueLabels();
      if (realtime.playing && isSequenceEnabled()) startPlayback();
    });
  }
  if (els.outroSelect) {
    els.outroSelect.addEventListener("change", async () => {
      await setOutroFromBuiltIn(els.outroSelect.value || "");
      refreshValueLabels();
      if (realtime.playing && isSequenceEnabled()) startPlayback();
    });
  }
  if (els.introFile) {
    els.introFile.addEventListener("change", async () => {
      const file = els.introFile.files?.[0];
      if (!file) return;
      await setIntroFromFile(file);
      refreshValueLabels();
      if (realtime.playing && isSequenceEnabled()) startPlayback();
    });
  }
  if (els.outroFile) {
    els.outroFile.addEventListener("change", async () => {
      const file = els.outroFile.files?.[0];
      if (!file) return;
      await setOutroFromFile(file);
      refreshValueLabels();
      if (realtime.playing && isSequenceEnabled()) startPlayback();
    });
  }

  const restartOnChange = (el) => {
    if (!el) return;
    el.addEventListener("change", () => {
      refreshValueLabels();
      refreshStatus();
      if (realtime.playing && isSequenceEnabled()) startPlayback();
    });
  };
  const restartOnInput = (el) => {
    if (!el) return;
    el.addEventListener("input", () => {
      refreshValueLabels();
      refreshStatus();
      if (realtime.playing && isSequenceEnabled()) startPlayback();
    });
  };

  restartOnChange(els.cheerToggle);
  restartOnChange(els.cheerSelect);
  restartOnChange(els.cheerWhen);
  restartOnInput(els.cheerLevel);

  restartOnChange(els.bed1Select);
  restartOnChange(els.bed1Where);
  restartOnChange(els.bed1Random);
  restartOnInput(els.bed1Level);

  restartOnChange(els.bed2Select);
  restartOnChange(els.bed2Where);
  restartOnChange(els.bed2Random);
  restartOnInput(els.bed2Level);

  const inputs = [
    els.crowdLevel,
    els.hotMic,
    els.fbFreq,
    els.ringQ,
    els.wall,
    els.room,
    els.outGain,
    els.fbDelayMs,
    els.fbTone,
    els.limit,
  ];
  for (const el of inputs) {
    el.addEventListener("input", () => {
      refreshValueLabels();
      applyRealtimeSettings();
    });
  }
}

async function init() {
  manifest = await loadManifest();
  const clips = [...(manifest.micCheck || []), ...(manifest.bandCheck || [])];
  setSelectOptions(els.clipSelect, clips, { noneLabel: "(choose a clip)" });
  setSelectOptions(els.crowdSelect, manifest.crowd || [], { noneLabel: "(none)" });
  if (els.introSelect) setSelectOptions(els.introSelect, clips, { noneLabel: "(none)" });
  if (els.outroSelect) setSelectOptions(els.outroSelect, clips, { noneLabel: "(none)" });
  if (els.cheerSelect) setSelectOptions(els.cheerSelect, manifest.crowd || [], { noneLabel: "(choose cheer)" });
  if (els.bed1Select) setSelectOptions(els.bed1Select, manifest.crowd || [], { noneLabel: "(none)" });
  if (els.bed2Select) setSelectOptions(els.bed2Select, manifest.crowd || [], { noneLabel: "(none)" });
  els.clipSelect.value = pickDefaultClip() || "";
  els.crowdSelect.value = "";

  // Default source: first built-in clip (if present).
  if (els.clipSelect.value) {
    try {
      await handleBuiltInClip(els.clipSelect.value);
    } catch (e) {
      console.warn(e);
      setState("Idle");
    }
  }

  refreshValueLabels();
  refreshStatus();
  wireEvents();
}

init();
