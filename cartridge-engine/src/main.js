import { buildCartridgeGraph, defaultSettings } from "./audio/graph.js?v=20260827.26";
import { encodeWavMono16 } from "./audio/wav.js";
import { PRESETS } from "./presets.js?v=20260827.26";

const els = {
  fileInput: document.querySelector("#fileInput"),
  playBtn: document.querySelector("#playBtn"),
  stopBtn: document.querySelector("#stopBtn"),
  exportBtn: document.querySelector("#exportBtn"),
  loopToggle: document.querySelector("#loopToggle"),

  quality: document.querySelector("#quality"),
  codec: document.querySelector("#codec"),
  grit: document.querySelector("#grit"),
  noise: document.querySelector("#noise"),
  preset: document.querySelector("#preset"),
  dither: document.querySelector("#dither"),
  noiseShaping: document.querySelector("#noiseShaping"),

  bits: document.querySelector("#bits"),
  rate: document.querySelector("#rate"),
  jitter: document.querySelector("#jitter"),
  lpHz: document.querySelector("#lpHz"),
  hpHz: document.querySelector("#hpHz"),
  preEmph: document.querySelector("#preEmph"),
  mulaw: document.querySelector("#mulaw"),
  codecMode: document.querySelector("#codecMode"),
  blockMs: document.querySelector("#blockMs"),
  sat: document.querySelector("#sat"),
  edge: document.querySelector("#edge"),
  noiseTrack: document.querySelector("#noiseTrack"),
  dcDrift: document.querySelector("#dcDrift"),
  speaker: document.querySelector("#speaker"),
  speakerModel: document.querySelector("#speakerModel"),
  hum: document.querySelector("#hum"),
  whine: document.querySelector("#whine"),
  outGain: document.querySelector("#outGain"),
  wet: document.querySelector("#wet"),
  ceiling: document.querySelector("#ceiling"),
  limiter: document.querySelector("#limiter"),

  bleepsEnable: document.querySelector("#bleepsEnable"),
  bleepsMix: document.querySelector("#bleepsMix"),
  bleepsRate: document.querySelector("#bleepsRate"),
  bleepsWave: document.querySelector("#bleepsWave"),
  bleepsTrigger: document.querySelector("#bleepsTrigger"),
  bleepsScale: document.querySelector("#bleepsScale"),
  bleepsVibrato: document.querySelector("#bleepsVibrato"),
  bleepsPitch: document.querySelector("#bleepsPitch"),
  microDelayMs: document.querySelector("#microDelayMs"),
  microDelayMix: document.querySelector("#microDelayMix"),
  verb: document.querySelector("#verb"),
  verbMs: document.querySelector("#verbMs"),

  qualityVal: document.querySelector("#qualityVal"),
  codecVal: document.querySelector("#codecVal"),
  gritVal: document.querySelector("#gritVal"),
  noiseVal: document.querySelector("#noiseVal"),

  bitsVal: document.querySelector("#bitsVal"),
  rateVal: document.querySelector("#rateVal"),
  jitterVal: document.querySelector("#jitterVal"),
  lpHzVal: document.querySelector("#lpHzVal"),
  hpHzVal: document.querySelector("#hpHzVal"),
  preEmphVal: document.querySelector("#preEmphVal"),
  mulawVal: document.querySelector("#mulawVal"),
  codecModeVal: document.querySelector("#codecModeVal"),
  blockMsVal: document.querySelector("#blockMsVal"),
  satVal: document.querySelector("#satVal"),
  edgeVal: document.querySelector("#edgeVal"),
  noiseTrackVal: document.querySelector("#noiseTrackVal"),
  dcDriftVal: document.querySelector("#dcDriftVal"),
  speakerVal: document.querySelector("#speakerVal"),
  speakerModelVal: document.querySelector("#speakerModelVal"),
  humVal: document.querySelector("#humVal"),
  whineVal: document.querySelector("#whineVal"),
  outGainVal: document.querySelector("#outGainVal"),
  wetVal: document.querySelector("#wetVal"),
  ceilingVal: document.querySelector("#ceilingVal"),
  limiterVal: document.querySelector("#limiterVal"),

  bleepsEnableVal: document.querySelector("#bleepsEnableVal"),
  bleepsMixVal: document.querySelector("#bleepsMixVal"),
  bleepsRateVal: document.querySelector("#bleepsRateVal"),
  bleepsWaveVal: document.querySelector("#bleepsWaveVal"),
  bleepsTriggerVal: document.querySelector("#bleepsTriggerVal"),
  bleepsScaleVal: document.querySelector("#bleepsScaleVal"),
  bleepsVibratoVal: document.querySelector("#bleepsVibratoVal"),
  bleepsPitchVal: document.querySelector("#bleepsPitchVal"),
  microDelayMsVal: document.querySelector("#microDelayMsVal"),
  microDelayMixVal: document.querySelector("#microDelayMixVal"),
  verbVal: document.querySelector("#verbVal"),
  verbMsVal: document.querySelector("#verbMsVal"),

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

function computeMacroTargets(primary) {
  const quality = clamp01(primary.quality ?? 0.55);
  const codec = clamp01(primary.codec ?? 0.25);
  const grit = clamp01(primary.grit ?? 0.25);
  const noise = clamp01(primary.noise ?? 0.15);

  const qCurve = Math.pow(1 - quality, 1.35);
  const bits = Math.round(16 - qCurve * 10);
  const rate = Math.round(48000 - qCurve * 40000);
  const lpHz = Math.round(18000 - qCurve * 15000);
  const jitter = clamp01(0.01 + qCurve * 0.16);

  const cCurve = Math.pow(codec, 1.15);
  const mulaw = clamp01(cCurve * 0.86);
  const blockMs = Math.round(2 + cCurve * cCurve * 18);
  const preEmph = clamp01(0.05 + cCurve * 0.38);

  const gCurve = Math.pow(grit, 1.25);
  const sat = clamp01(0.04 + gCurve * 0.58);
  const edge = clamp01(0.03 + gCurve * 0.5);
  const dcDrift = clamp01(0.01 + gCurve * 0.16);
  const hum = clamp01(gCurve * 0.12);
  const whine = clamp01(0.025 + gCurve * 0.2);
  const outGain = 0.98 - gCurve * 0.08;

  return {
    bits,
    rate,
    lpHz,
    jitter,
    mulaw,
    blockMs,
    preEmph,
    sat,
    edge,
    dcDrift,
    hum,
    whine,
    noise,
    outGain,
  };
}

function applyMacrosFromPrimaryToAdvanced() {
  const targets = computeMacroTargets(readSettingsFromUI());
  els.bits.value = String(targets.bits);
  els.rate.value = String(targets.rate);
  els.lpHz.value = String(targets.lpHz);
  els.jitter.value = String(targets.jitter);
  els.mulaw.value = String(targets.mulaw);
  els.blockMs.value = String(targets.blockMs);
  els.preEmph.value = String(targets.preEmph);
  els.sat.value = String(targets.sat);
  els.edge.value = String(targets.edge);
  els.dcDrift.value = String(targets.dcDrift);
  els.hum.value = String(targets.hum);
  els.whine.value = String(targets.whine);
  els.outGain.value = String(targets.outGain);
  refreshValueLabels();
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
let audioDataSeed = 0xfeedc0de;

let realtime = {
  ctx: null,
  graph: null,
  src: null,
  playing: false,
};

function readSettingsFromUI() {
  settings = {
    quality: clamp01(Number(els.quality.value)),
    codec: clamp01(Number(els.codec.value)),
    grit: clamp01(Number(els.grit.value)),
    noise: clamp01(Number(els.noise.value)),
    dither: Boolean(els.dither.checked),
    noiseShaping: Boolean(els.noiseShaping.checked),

    bits: Number(els.bits.value),
    rate: Number(els.rate.value),
    jitter: clamp01(Number(els.jitter.value)),
    lpHz: Number(els.lpHz.value),
    hpHz: Number(els.hpHz.value),
    preEmph: clamp01(Number(els.preEmph.value)),
    mulaw: clamp01(Number(els.mulaw.value)),
    codecMode: els.codecMode.value || "adpcm",
    blockMs: Number(els.blockMs.value),
    sat: clamp01(Number(els.sat.value)),
    edge: clamp01(Number(els.edge.value)),
    noiseTrack: clamp01(Number(els.noiseTrack.value)),
    dcDrift: clamp01(Number(els.dcDrift.value)),
    speaker: clamp01(Number(els.speaker.value)),
    speakerModel: els.speakerModel.value || "handheld",
    hum: clamp01(Number(els.hum.value)),
    whine: clamp01(Number(els.whine.value)),
    outGain: Number(els.outGain.value),
    wet: clamp01(Number(els.wet.value)),
    ceiling: Number(els.ceiling.value),
    limiter: clamp01(Number(els.limiter.value)),

    bleepsEnable: Boolean(els.bleepsEnable.checked),
    bleepsMix: clamp01(Number(els.bleepsMix.value)),
    bleepsRate: Number(els.bleepsRate.value),
    bleepsWave: els.bleepsWave.value || "random",
    bleepsTrigger: els.bleepsTrigger.value || "transient",
    bleepsScale: els.bleepsScale.value || "minor",
    bleepsVibrato: clamp01(Number(els.bleepsVibrato.value)),
    bleepsPitch: clamp01(Number(els.bleepsPitch.value)),
    microDelayMs: Number(els.microDelayMs.value),
    microDelayMix: clamp01(Number(els.microDelayMix.value)),
    verb: clamp01(Number(els.verb.value)),
    verbMs: Number(els.verbMs.value),
  };
  return settings;
}

function writeSettingsToUI(next) {
  els.quality.value = String(next.quality ?? 0.55);
  els.codec.value = String(next.codec ?? 0.25);
  els.grit.value = String(next.grit ?? 0.25);
  els.noise.value = String(next.noise ?? 0.15);
  els.dither.checked = Boolean(next.dither ?? true);
  els.noiseShaping.checked = Boolean(next.noiseShaping ?? false);

  els.bits.value = String(next.bits ?? 10);
  els.rate.value = String(next.rate ?? 24000);
  els.jitter.value = String(next.jitter ?? 0.05);
  els.lpHz.value = String(next.lpHz ?? 9000);
  els.hpHz.value = String(next.hpHz ?? 70);
  els.preEmph.value = String(next.preEmph ?? 0.2);
  els.mulaw.value = String(next.mulaw ?? 0.25);
  els.codecMode.value = next.codecMode ?? "adpcm";
  els.blockMs.value = String(next.blockMs ?? 8);
  els.sat.value = String(next.sat ?? 0.25);
  els.edge.value = String(next.edge ?? 0.25);
  els.noiseTrack.value = String(next.noiseTrack ?? 0.6);
  els.dcDrift.value = String(next.dcDrift ?? 0.15);
  els.speaker.value = String(next.speaker ?? 0.45);
  els.speakerModel.value = next.speakerModel ?? "handheld";
  els.hum.value = String(next.hum ?? 0.08);
  els.whine.value = String(next.whine ?? 0.15);
  els.outGain.value = String(next.outGain ?? 0.95);
  els.wet.value = String(next.wet ?? 1);
  els.ceiling.value = String(next.ceiling ?? 0.92);
  els.limiter.value = String(next.limiter ?? 0.35);

  els.bleepsEnable.checked = Boolean(next.bleepsEnable ?? false);
  els.bleepsMix.value = String(next.bleepsMix ?? 0.12);
  els.bleepsRate.value = String(next.bleepsRate ?? 3);
  els.bleepsWave.value = next.bleepsWave ?? "pulse";
  els.bleepsTrigger.value = next.bleepsTrigger ?? "transient";
  els.bleepsScale.value = next.bleepsScale ?? "minor";
  els.bleepsVibrato.value = String(next.bleepsVibrato ?? 0.12);
  els.bleepsPitch.value = String(next.bleepsPitch ?? 0.55);
  els.microDelayMs.value = String(next.microDelayMs ?? 8);
  els.microDelayMix.value = String(next.microDelayMix ?? 0.06);
  els.verb.value = String(next.verb ?? 0.08);
  els.verbMs.value = String(next.verbMs ?? 45);

  refreshValueLabels();
}

function refreshValueLabels() {
  const s = readSettingsFromUI();
  const m = computeMacroTargets(s);

  els.qualityVal.textContent = `${m.bits}b @ ${Math.round(m.rate / 100) / 10}kHz`;
  els.codecVal.textContent = `${els.codecMode.value.toUpperCase()} ${pct01(m.mulaw)} | ${Math.round(m.blockMs)}ms blocks`;
  els.gritVal.textContent = `sat ${pct01(m.sat)} | jitter ${pct01(m.jitter)}`;
  els.noiseVal.textContent = `bed ${pct01(s.noise)} | whine ${pct01(Number(els.whine.value))}`;

  els.bitsVal.textContent = `${Math.round(Number(els.bits.value))}b`;
  els.rateVal.textContent = `${Math.round(Number(els.rate.value) / 100) / 10}kHz`;
  els.jitterVal.textContent = pct01(Number(els.jitter.value));
  els.lpHzVal.textContent = `${Math.round(Number(els.lpHz.value))}Hz`;
  els.hpHzVal.textContent = `${Math.round(Number(els.hpHz.value))}Hz`;
  els.preEmphVal.textContent = pct01(Number(els.preEmph.value));
  els.mulawVal.textContent = pct01(Number(els.mulaw.value));
  els.codecModeVal.textContent = els.codecMode.value.toUpperCase();
  els.blockMsVal.textContent = `${Math.round(Number(els.blockMs.value))}ms`;
  els.satVal.textContent = pct01(Number(els.sat.value));
  els.edgeVal.textContent = pct01(Number(els.edge.value));
  els.noiseTrackVal.textContent = pct01(Number(els.noiseTrack.value));
  els.dcDriftVal.textContent = pct01(Number(els.dcDrift.value));
  els.speakerVal.textContent = pct01(Number(els.speaker.value));
  els.speakerModelVal.textContent = els.speakerModel.options[els.speakerModel.selectedIndex]?.textContent ?? els.speakerModel.value;
  els.humVal.textContent = pct01(Number(els.hum.value));
  els.whineVal.textContent = pct01(Number(els.whine.value));
  els.outGainVal.textContent = `${round1(Number(els.outGain.value))}x`;
  els.wetVal.textContent = pct01(Number(els.wet.value));
  els.ceilingVal.textContent = `${round1(Number(els.ceiling.value))}`;
  els.limiterVal.textContent = pct01(Number(els.limiter.value));

  els.bleepsEnableVal.textContent = els.bleepsEnable.checked ? "on" : "off";
  els.bleepsMixVal.textContent = pct01(Number(els.bleepsMix.value));
  els.bleepsRateVal.textContent = `${round1(Number(els.bleepsRate.value))}/s`;
  els.bleepsWaveVal.textContent = els.bleepsWave.value || "random";
  els.bleepsTriggerVal.textContent = els.bleepsTrigger.value;
  els.bleepsScaleVal.textContent = els.bleepsScale.value;
  els.bleepsVibratoVal.textContent = pct01(Number(els.bleepsVibrato.value));
  els.bleepsPitchVal.textContent = pct01(Number(els.bleepsPitch.value));
  els.microDelayMsVal.textContent = `${round1(Number(els.microDelayMs.value))}ms`;
  els.microDelayMixVal.textContent = pct01(Number(els.microDelayMix.value));
  els.verbVal.textContent = pct01(Number(els.verb.value));
  els.verbMsVal.textContent = `${Math.round(Number(els.verbMs.value))}ms`;
}

async function ensureRealtimeGraph() {
  if (!audioBuffer) return;
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
  realtime.graph = await buildCartridgeGraph(realtime.ctx, { seed: audioDataSeed });
  realtime.graph.output.connect(realtime.ctx.destination);
  realtime.graph.applySettings(readSettingsFromUI(), { ramp: 0.03 });
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
  src.loop = Boolean(els.loopToggle.checked);
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

  const s = readSettingsFromUI();
  const offline = new OfflineAudioContext({
    numberOfChannels: 1,
    length: audioBuffer.length,
    sampleRate: audioBuffer.sampleRate,
  });
  const graph = await buildCartridgeGraph(offline, { seed: audioDataSeed });
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
  a.download = `${base} - Cartridge Engine.wav`;
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
  els.quality.addEventListener("input", () => {
    applyMacrosFromPrimaryToAdvanced();
    applyToGraphAndMarkCustom();
  });
  els.codec.addEventListener("input", () => {
    applyMacrosFromPrimaryToAdvanced();
    applyToGraphAndMarkCustom();
  });
  els.grit.addEventListener("input", () => {
    applyMacrosFromPrimaryToAdvanced();
    applyToGraphAndMarkCustom();
  });
  els.noise.addEventListener("input", applyToGraphAndMarkCustom);
  for (const el of [els.dither, els.noiseShaping]) el.addEventListener("change", applyToGraphAndMarkCustom);

  const adv = [
    els.bits,
    els.rate,
    els.jitter,
    els.lpHz,
    els.hpHz,
    els.preEmph,
    els.mulaw,
    els.codecMode,
    els.blockMs,
    els.sat,
    els.edge,
    els.noiseTrack,
    els.dcDrift,
    els.speaker,
    els.speakerModel,
    els.hum,
    els.whine,
    els.outGain,
    els.wet,
    els.ceiling,
    els.limiter,
    els.bleepsEnable,
    els.bleepsMix,
    els.bleepsRate,
    els.bleepsWave,
    els.bleepsTrigger,
    els.bleepsScale,
    els.bleepsVibrato,
    els.bleepsPitch,
    els.microDelayMs,
    els.microDelayMix,
    els.verb,
    els.verbMs,
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
    for (const [key, value] of Object.entries(preset)) {
      const control = els[key];
      if (!control || ["quality", "codec", "grit", "noise"].includes(key)) continue;
      if (control.type === "checkbox") control.checked = Boolean(value);
      else control.value = String(value);
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
