import { buildCommsGraph, defaultSettings } from "./audio/graph.js";
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
  drive: document.querySelector("#drive"),
  glitch: document.querySelector("#glitch"),
  noise: document.querySelector("#noise"),
  preset: document.querySelector("#preset"),
  alarmTone: document.querySelector("#alarmTone"),

  hpHz: document.querySelector("#hpHz"),
  lpHz: document.querySelector("#lpHz"),
  midHumpDb: document.querySelector("#midHumpDb"),
  midFreq: document.querySelector("#midFreq"),
  comp: document.querySelector("#comp"),
  bits: document.querySelector("#bits"),
  rate: document.querySelector("#rate"),
  packet: document.querySelector("#packet"),
  packetMs: document.querySelector("#packetMs"),
  hum: document.querySelector("#hum"),
  hiss: document.querySelector("#hiss"),
  toneMix: document.querySelector("#toneMix"),
  ceiling: document.querySelector("#ceiling"),
  outGain: document.querySelector("#outGain"),
  echoMix: document.querySelector("#echoMix"),
  echoMs: document.querySelector("#echoMs"),
  echoFb: document.querySelector("#echoFb"),
  echoTone: document.querySelector("#echoTone"),
  verbMix: document.querySelector("#verbMix"),
  verbMs: document.querySelector("#verbMs"),
  verbDamp: document.querySelector("#verbDamp"),

  modeVal: document.querySelector("#modeVal"),
  bandwidthVal: document.querySelector("#bandwidthVal"),
  driveVal: document.querySelector("#driveVal"),
  glitchVal: document.querySelector("#glitchVal"),
  noiseVal: document.querySelector("#noiseVal"),

  hpHzVal: document.querySelector("#hpHzVal"),
  lpHzVal: document.querySelector("#lpHzVal"),
  midHumpDbVal: document.querySelector("#midHumpDbVal"),
  midFreqVal: document.querySelector("#midFreqVal"),
  compVal: document.querySelector("#compVal"),
  bitsVal: document.querySelector("#bitsVal"),
  rateVal: document.querySelector("#rateVal"),
  packetVal: document.querySelector("#packetVal"),
  packetMsVal: document.querySelector("#packetMsVal"),
  humVal: document.querySelector("#humVal"),
  hissVal: document.querySelector("#hissVal"),
  toneMixVal: document.querySelector("#toneMixVal"),
  ceilingVal: document.querySelector("#ceilingVal"),
  outGainVal: document.querySelector("#outGainVal"),
  echoGroupVal: document.querySelector("#echoGroupVal"),
  echoMixVal: document.querySelector("#echoMixVal"),
  echoMsVal: document.querySelector("#echoMsVal"),
  echoFbVal: document.querySelector("#echoFbVal"),
  echoToneVal: document.querySelector("#echoToneVal"),
  verbGroupVal: document.querySelector("#verbGroupVal"),
  verbMixVal: document.querySelector("#verbMixVal"),
  verbMsVal: document.querySelector("#verbMsVal"),
  verbDampVal: document.querySelector("#verbDampVal"),

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
    drive: parseFloat(els.drive.value),
    glitch: parseFloat(els.glitch.value),
    noise: parseFloat(els.noise.value),
  };
}

function computeMacroTargets(primary) {
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

  return {
    hpHz,
    lpHz,
    midHumpDb,
    midFreq,
    comp,
    bits,
    rate,
    packet,
    packetMs,
    hum,
    hiss,
    toneMix,
    ceiling,
    outGain,
    echoMix,
    echoMs,
    echoFb,
    echoTone,
    verbMix,
    verbMs,
    verbDamp,
  };
}

function readSettingsFromUI() {
  return {
    mode: els.mode.value,
    bandwidth: parseFloat(els.bandwidth.value),
    drive: parseFloat(els.drive.value),
    glitch: parseFloat(els.glitch.value),
    noise: parseFloat(els.noise.value),
    alarmTone: Boolean(els.alarmTone.checked),

    hpHz: parseFloat(els.hpHz.value),
    lpHz: parseFloat(els.lpHz.value),
    midHumpDb: parseFloat(els.midHumpDb.value),
    midFreq: parseFloat(els.midFreq.value),
    comp: parseFloat(els.comp.value),
    bits: parseFloat(els.bits.value),
    rate: parseFloat(els.rate.value),
    packet: parseFloat(els.packet.value),
    packetMs: parseFloat(els.packetMs.value),
    hum: parseFloat(els.hum.value),
    hiss: parseFloat(els.hiss.value),
    toneMix: parseFloat(els.toneMix.value),
    ceiling: parseFloat(els.ceiling.value),
    outGain: parseFloat(els.outGain.value),
    echoMix: parseFloat(els.echoMix.value),
    echoMs: parseFloat(els.echoMs.value),
    echoFb: parseFloat(els.echoFb.value),
    echoTone: parseFloat(els.echoTone.value),
    verbMix: parseFloat(els.verbMix.value),
    verbMs: parseFloat(els.verbMs.value),
    verbDamp: parseFloat(els.verbDamp.value),
  };
}

function writeSettingsToUI(s) {
  els.mode.value = s.mode ?? "landline";
  els.bandwidth.value = s.bandwidth ?? 0.4;
  els.drive.value = s.drive ?? 0.35;
  els.glitch.value = s.glitch ?? 0.2;
  els.noise.value = s.noise ?? 0.18;
  els.alarmTone.checked = Boolean(s.alarmTone);

  els.hpHz.value = s.hpHz ?? 280;
  els.lpHz.value = s.lpHz ?? 3400;
  els.midHumpDb.value = s.midHumpDb ?? 3.5;
  els.midFreq.value = s.midFreq ?? 1850;
  els.comp.value = s.comp ?? 0.45;
  els.bits.value = s.bits ?? 12;
  els.rate.value = s.rate ?? 24000;
  els.packet.value = s.packet ?? 0.2;
  els.packetMs.value = s.packetMs ?? 28;
  els.hum.value = s.hum ?? 0.25;
  els.hiss.value = s.hiss ?? 0.22;
  els.toneMix.value = s.toneMix ?? 0.35;
  els.ceiling.value = s.ceiling ?? 0.92;
  els.outGain.value = s.outGain ?? 0.95;
  els.echoMix.value = s.echoMix ?? 0;
  els.echoMs.value = s.echoMs ?? 180;
  els.echoFb.value = s.echoFb ?? 0.28;
  els.echoTone.value = s.echoTone ?? 0.55;
  els.verbMix.value = s.verbMix ?? 0;
  els.verbMs.value = s.verbMs ?? 240;
  els.verbDamp.value = s.verbDamp ?? 0.45;
  refreshValueLabels();
}

function refreshValueLabels() {
  const s = readSettingsFromUI();
  const modeName =
    s.mode === "cell"
      ? "Cellphone"
      : s.mode === "intercom"
        ? "Intercom"
        : s.mode === "pa"
          ? "PA"
          : s.mode === "alarm"
            ? "Alarm"
            : "Landline";
  els.modeVal.textContent = modeName;
  els.bandwidthVal.textContent = pct01(s.bandwidth);
  els.driveVal.textContent = pct01(s.drive);
  els.glitchVal.textContent = pct01(s.glitch);
  els.noiseVal.textContent = pct01(s.noise);

  els.hpHzVal.textContent = fmtHz(s.hpHz);
  els.lpHzVal.textContent = fmtHz(s.lpHz);
  els.midHumpDbVal.textContent = `${Number(s.midHumpDb).toFixed(1)} dB`;
  els.midFreqVal.textContent = fmtHz(s.midFreq);
  els.compVal.textContent = pct01(s.comp);
  els.bitsVal.textContent = `${Math.round(s.bits)}-bit`;
  els.rateVal.textContent = fmtHz(s.rate);
  els.packetVal.textContent = pct01(s.packet);
  els.packetMsVal.textContent = `${Math.round(s.packetMs)} ms`;
  els.humVal.textContent = pct01(s.hum);
  els.hissVal.textContent = pct01(s.hiss);
  els.toneMixVal.textContent = pct01(s.toneMix);
  els.ceilingVal.textContent = `${Math.round(s.ceiling * 100)}%`;
  els.outGainVal.textContent = `${s.outGain.toFixed(2)}x`;

  els.echoMixVal.textContent = pct01(s.echoMix);
  els.echoMsVal.textContent = `${Math.round(s.echoMs)} ms`;
  els.echoFbVal.textContent = pct01(s.echoFb);
  els.echoToneVal.textContent = pct01(s.echoTone);
  els.verbMixVal.textContent = pct01(s.verbMix);
  els.verbMsVal.textContent = `${Math.round(s.verbMs)} ms`;
  els.verbDampVal.textContent = pct01(s.verbDamp);

  if (els.echoGroupVal) els.echoGroupVal.textContent = s.echoMix > 0.001 ? `On` : `Off`;
  if (els.verbGroupVal) els.verbGroupVal.textContent = s.verbMix > 0.001 ? `On` : `Off`;
}

function applyMacrosFromPrimaryToAdvanced() {
  const primary = readPrimaryFromUI();
  const t = computeMacroTargets(primary);
  els.hpHz.value = t.hpHz;
  els.lpHz.value = t.lpHz;
  els.midHumpDb.value = t.midHumpDb;
  els.midFreq.value = t.midFreq;
  els.comp.value = t.comp;
  els.bits.value = t.bits;
  els.rate.value = t.rate;
  els.packet.value = t.packet;
  els.packetMs.value = t.packetMs;
  els.hum.value = t.hum;
  els.hiss.value = t.hiss;
  els.toneMix.value = t.toneMix;
  els.ceiling.value = t.ceiling;
  els.outGain.value = t.outGain;
  els.echoMix.value = t.echoMix;
  els.echoMs.value = t.echoMs;
  els.echoFb.value = t.echoFb;
  els.echoTone.value = t.echoTone;
  els.verbMix.value = t.verbMix;
  els.verbMs.value = t.verbMs;
  els.verbDamp.value = t.verbDamp;
  refreshValueLabels();
}

async function ensureRealtimeGraph() {
  if (realtime.ctx && realtime.graph) return;
  if (!audioBuffer) throw new Error("No audio loaded");
  realtime.ctx = new (window.AudioContext || window.webkitAudioContext)({
    numberOfChannels: 1,
    sampleRate: audioBuffer.sampleRate,
  });
  realtime.graph = await buildCommsGraph(realtime.ctx, { seed: audioDataSeed });
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
  const graph = await buildCommsGraph(offline, { seed: audioDataSeed });
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
  a.download = `${base} - Comms Engine.wav`;
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
  for (const el of [els.mode, els.bandwidth, els.drive, els.glitch, els.noise]) {
    const evt = el.tagName === "SELECT" ? "change" : "input";
    el.addEventListener(evt, () => {
      applyMacrosFromPrimaryToAdvanced();
      applyToGraphAndMarkCustom();
    });
  }

  const adv = [
    els.alarmTone,
    els.hpHz,
    els.lpHz,
    els.midHumpDb,
    els.midFreq,
    els.comp,
    els.bits,
    els.rate,
    els.packet,
    els.packetMs,
    els.hum,
    els.hiss,
    els.toneMix,
    els.ceiling,
    els.outGain,
    els.echoMix,
    els.echoMs,
    els.echoFb,
    els.echoTone,
    els.verbMix,
    els.verbMs,
    els.verbDamp,
  ];
  for (const el of adv) {
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
    // Re-apply any explicit advanced overrides from the preset.
    const overrideKeys = [
      "alarmTone",
      "hpHz",
      "lpHz",
      "midHumpDb",
      "midFreq",
      "comp",
      "bits",
      "rate",
      "packet",
      "packetMs",
      "hum",
      "hiss",
      "toneMix",
      "ceiling",
      "outGain",
      "echoMix",
      "echoMs",
      "echoFb",
      "echoTone",
      "verbMix",
      "verbMs",
      "verbDamp",
    ];
    for (const k of overrideKeys) {
      if (!(k in preset)) continue;
      const v = preset[k];
      if (k === "alarmTone") els.alarmTone.checked = Boolean(v);
      else if (k === "hpHz") els.hpHz.value = v;
      else if (k === "lpHz") els.lpHz.value = v;
      else if (k === "midHumpDb") els.midHumpDb.value = v;
      else if (k === "midFreq") els.midFreq.value = v;
      else if (k === "comp") els.comp.value = v;
      else if (k === "bits") els.bits.value = v;
      else if (k === "rate") els.rate.value = v;
      else if (k === "packet") els.packet.value = v;
      else if (k === "packetMs") els.packetMs.value = v;
      else if (k === "hum") els.hum.value = v;
      else if (k === "hiss") els.hiss.value = v;
      else if (k === "toneMix") els.toneMix.value = v;
      else if (k === "ceiling") els.ceiling.value = v;
      else if (k === "outGain") els.outGain.value = v;
      else if (k === "echoMix") els.echoMix.value = v;
      else if (k === "echoMs") els.echoMs.value = v;
      else if (k === "echoFb") els.echoFb.value = v;
      else if (k === "echoTone") els.echoTone.value = v;
      else if (k === "verbMix") els.verbMix.value = v;
      else if (k === "verbMs") els.verbMs.value = v;
      else if (k === "verbDamp") els.verbDamp.value = v;
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
