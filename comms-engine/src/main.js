import { buildCommsGraph, defaultSettings } from "./audio/graph.js?v=20260827.26";
import { encodeWavMono16 } from "./audio/wav.js";
import { PRESETS } from "./presets.js?v=20260827.26";

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
  character: document.querySelector("#character"),
  distance: document.querySelector("#distance"),
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
  transducer: document.querySelector("#transducer"),
  lineAge: document.querySelector("#lineAge"),
  duplex: document.querySelector("#duplex"),
  speakerRattle: document.querySelector("#speakerRattle"),
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
  characterVal: document.querySelector("#characterVal"),
  distanceVal: document.querySelector("#distanceVal"),

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
  transducerVal: document.querySelector("#transducerVal"),
  lineAgeVal: document.querySelector("#lineAgeVal"),
  duplexVal: document.querySelector("#duplexVal"),
  speakerRattleVal: document.querySelector("#speakerRattleVal"),
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
    character: parseFloat(els.character.value),
    distance: parseFloat(els.distance.value),
  };
}

function computeMacroTargets(primary) {
  const mode = primary.mode ?? "landline";
  const bandwidth = clamp01(primary.bandwidth ?? 0.4);
  const drive = clamp01(primary.drive ?? 0.35);
  const glitch = clamp01(primary.glitch ?? 0.2);
  const noise = clamp01(primary.noise ?? 0.18);
  const character = clamp01(primary.character ?? 0.45);
  const distance = clamp01(primary.distance ?? 0.15);

  const narrow = Math.pow(1 - bandwidth, 1.35);
  const drv = Math.pow(drive, 1.25);
  const g = Math.pow(glitch, 1.35);
  const n = Math.pow(noise, 1.2);
  const ch = Math.pow(character, 0.9);

  const base =
    mode === "cell"
      ? { hp: 170, hpR: 390, lp: 7200, lpR: 4600, hump: 1.4, humpR: 3.8, mid: 2050, midR: 550, comp: 0.58, out: 0.98, ceil: 0.92, device: 0.2, line: 0.04, duplex: 0.08, rattle: 0.04, bits: 13, rate: 32000, rateR: 25000 }
      : mode === "intercom"
        ? { hp: 310, hpR: 420, lp: 4100, lpR: 1900, hump: 4.6, humpR: 5.8, mid: 1950, midR: 420, comp: 0.65, out: 0.94, ceil: 0.9, device: 0.68, line: 0.28, duplex: 0.48, rattle: 0.5, bits: 15, rate: 44000, rateR: 30000 }
        : mode === "pa"
          ? { hp: 120, hpR: 330, lp: 9800, lpR: 5200, hump: 1.2, humpR: 3.0, mid: 1420, midR: 400, comp: 0.46, out: 0.94, ceil: 0.9, device: 0.58, line: 0.1, duplex: 0.04, rattle: 0.46, bits: 16, rate: 48000, rateR: 28000 }
          : mode === "alarm"
            ? { hp: 230, hpR: 390, lp: 8200, lpR: 4300, hump: 1.4, humpR: 3.2, mid: 1740, midR: 620, comp: 0.48, out: 0.94, ceil: 0.9, device: 0.62, line: 0.16, duplex: 0.02, rattle: 0.28, bits: 15, rate: 46000, rateR: 30000 }
            : { hp: 250, hpR: 330, lp: 3900, lpR: 1500, hump: 3.0, humpR: 5.0, mid: 1820, midR: 360, comp: 0.52, out: 0.94, ceil: 0.92, device: 0.52, line: 0.3, duplex: 0.02, rattle: 0.16, bits: 14, rate: 44000, rateR: 30000 };

  const hpHz = Math.round(base.hp + narrow * base.hpR);
  const lpHz = Math.round(base.lp - narrow * base.lpR);
  const midHumpDb = Math.round((base.hump + narrow * base.humpR) * 20) / 20;
  const midFreq = Math.round(base.mid + (0.55 - narrow) * base.midR);

  const comp = clamp01(base.comp + drv * 0.38);
  const bits = Math.round((1 - g) * (base.bits - 5) + 5);
  const rate = Math.round(base.rate - g * base.rateR);

  const packetScale = mode === "cell" ? 0.72 : mode === "alarm" ? 0.35 : 0.25;
  const packet = clamp01(g * packetScale);
  const packetMs = Math.round(10 + g * (mode === "cell" ? 120 : 75));

  const hum = clamp01(0.06 + n * (mode === "intercom" ? 0.55 : 0.4));
  const hiss = clamp01(0.08 + n * 0.55);
  const toneMix = clamp01((mode === "alarm" ? 0.45 : 0.22) + n * 0.15);
  const transducer = clamp01(base.device * 0.5 + ch * 0.72);
  const lineAge = clamp01(base.line + drv * 0.24 + n * 0.18);
  const duplex = clamp01(base.duplex + g * (mode === "intercom" ? 0.35 : 0.18) + ch * (mode === "intercom" ? 0.12 : 0.03));
  const speakerRattle = clamp01(base.rattle * (0.35 + ch * 0.9) + drv * (mode === "pa" || mode === "intercom" ? 0.28 : 0.12));

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
    transducer,
    lineAge,
    duplex,
    speakerRattle,
    distance,
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
    character: parseFloat(els.character.value),
    distance: parseFloat(els.distance.value),
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
    transducer: parseFloat(els.transducer.value),
    lineAge: parseFloat(els.lineAge.value),
    duplex: parseFloat(els.duplex.value),
    speakerRattle: parseFloat(els.speakerRattle.value),
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
  els.character.value = s.character ?? 0.45;
  els.distance.value = s.distance ?? 0.15;
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
  els.transducer.value = s.transducer ?? 0.45;
  els.lineAge.value = s.lineAge ?? 0.2;
  els.duplex.value = s.duplex ?? 0.08;
  els.speakerRattle.value = s.speakerRattle ?? 0.12;
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
  els.characterVal.textContent = pct01(s.character);
  els.distanceVal.textContent = pct01(s.distance);

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
  els.transducerVal.textContent = pct01(s.transducer);
  els.lineAgeVal.textContent = pct01(s.lineAge);
  els.duplexVal.textContent = pct01(s.duplex);
  els.speakerRattleVal.textContent = pct01(s.speakerRattle);
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
  els.transducer.value = t.transducer;
  els.lineAge.value = t.lineAge;
  els.duplex.value = t.duplex;
  els.speakerRattle.value = t.speakerRattle;
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
  for (const el of [els.mode, els.bandwidth, els.drive, els.glitch, els.noise, els.character, els.distance]) {
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
    els.transducer,
    els.lineAge,
    els.duplex,
    els.speakerRattle,
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
      "transducer",
      "lineAge",
      "duplex",
      "speakerRattle",
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
      else if (k === "transducer") els.transducer.value = v;
      else if (k === "lineAge") els.lineAge.value = v;
      else if (k === "duplex") els.duplex.value = v;
      else if (k === "speakerRattle") els.speakerRattle.value = v;
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
