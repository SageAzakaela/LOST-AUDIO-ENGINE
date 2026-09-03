import { buildTapeGraph, defaultSettings as tapeDefaults } from "../tape-engine/src/audio/graph.js?v=20260827.21";
import { buildTelevisionGraph, defaultSettings as televisionDefaults } from "../television-engine/src/audio/graph.js?v=20260827.2";
import { buildOcclusionGraph, defaultSettings as occlusionDefaults } from "../occlusion-engine/src/audio/graph.js?v=20260828.20";
import { buildTransmissionGraph, defaultSettings as transmissionDefaults } from "../src/audio/graph.js?v=20260827.4";
import { buildCommsGraph, defaultSettings as commsDefaults } from "../comms-engine/src/audio/graph.js?v=20260827.26";
import { buildConferenceGraph, defaultSettings as conferenceDefaults } from "../conference-engine/src/audio/graph.js?v=20260827.24";
import { buildCartridgeGraph, defaultSettings as cartridgeDefaults } from "../cartridge-engine/src/audio/graph.js?v=20260827.26";
import { buildCdGraph, defaultSettings as cdDefaults } from "../cd-engine/src/audio/graph.js?v=20260827.6";
import { buildCamcorderGraph, defaultSettings as camcorderDefaults } from "../camcorder-engine/src/audio/graph.js?v=20260828.28";

const sampleRate = 48000;
const seconds = 1.5;
const frames = Math.round(sampleRate * seconds);
const seed = 0x51a7e;

function makeInput(context, kind = "signal") {
  const buffer = context.createBuffer(1, frames, sampleRate);
  const data = buffer.getChannelData(0);
  if (kind === "silence") return buffer;
  const gain = kind === "loud" ? 0.72 : kind === "quiet" ? 0.008 : 0.26;
  let noiseSeed = 0x9e3779b9;
  for (let i = 0; i < data.length; i++) {
    const t = i / sampleRate;
    noiseSeed = (Math.imul(noiseSeed, 1664525) + 1013904223) >>> 0;
    const noise = (noiseSeed / 0xffffffff) * 2 - 1;
    const burstPhase = i % 12000;
    const burst = burstPhase < 900 ? Math.exp(-burstPhase / 180) * noise * 0.5 : 0;
    data[i] = Math.max(-0.95, Math.min(0.95, gain * (
      Math.sin(2 * Math.PI * 83 * t) * 0.32 +
      Math.sin(2 * Math.PI * 317 * t) * 0.26 +
      Math.sin(2 * Math.PI * 1187 * t) * 0.2 +
      Math.sin(2 * Math.PI * 6131 * t) * 0.14 + burst
    )));
  }
  return buffer;
}

async function render(engine, settings, kind) {
  const context = new OfflineAudioContext(1, frames, sampleRate);
  const graph = await engine.build(context, { seed, settings });
  graph.output.connect(context.destination);
  graph.applySettings(settings, { time: 0, ramp: 0.001 });
  graph.reset?.(seed);
  if (settings.__triggerDamage) graph.triggerDamage?.(1);
  if (settings.__triggerSkip) graph.triggerSkip?.(1);
  const source = new AudioBufferSourceNode(context, { buffer: makeInput(context, kind) });
  source.connect(graph.input);
  source.start(0);
  const rendered = await context.startRendering();
  return new Float32Array(rendered.getChannelData(0));
}

function compare(a, b) {
  const start = Math.round(sampleRate * 0.08);
  let ea = 0;
  let eb = 0;
  let ed = 0;
  let peakDiff = 0;
  let peakLow = 0;
  let peakHigh = 0;
  let nonFiniteSamples = 0;
  for (let i = start; i < a.length; i++) {
    if (!Number.isFinite(a[i]) || !Number.isFinite(b[i])) {
      nonFiniteSamples++;
      continue;
    }
    ea += a[i] * a[i];
    eb += b[i] * b[i];
    peakLow = Math.max(peakLow, Math.abs(a[i]));
    peakHigh = Math.max(peakHigh, Math.abs(b[i]));
    const d = b[i] - a[i];
    ed += d * d;
    peakDiff = Math.max(peakDiff, Math.abs(d));
  }
  const count = a.length - start;
  if (nonFiniteSamples > 0) {
    return {
      influencePercent: null,
      diffRms: null,
      peakDiff: null,
      peakLow: null,
      peakHigh: null,
      rmsLow: null,
      rmsHigh: null,
      nonFiniteSamples,
      classification: "NON_FINITE",
    };
  }
  const rmsA = Math.sqrt(ea / count);
  const rmsB = Math.sqrt(eb / count);
  const diffRms = Math.sqrt(ed / count);
  const reference = Math.max(1e-8, rmsA, rmsB);
  const influencePercent = diffRms * 100 / reference;
  return {
    influencePercent,
    diffRms,
    peakDiff,
    peakLow,
    peakHigh,
    rmsLow: rmsA,
    rmsHigh: rmsB,
    nonFiniteSamples: 0,
    classification: influencePercent < 0.1 ? "DEAD" : influencePercent < 1 ? "TOO_SUBTLE" : "ACTIVE",
  };
}

const engines = {
  tape: {
    build: buildTapeGraph,
    defaults: { ...tapeDefaults(), sfxEnable: false, hiss: 0, hum: 0, dropout: 0 },
    cases: {
      hpHz: [10, 240], lpHz: [1800, 18000],
      headBumpDb: [0, 10], headBumpHz: [40, 160, { headBumpDb: 10 }],
      drive: [0, 1], comp: [0, 1, {}, "loud"],
      speed: [0.85, 1.15, { wow: 1, wowDepthMs: 10, flutterDepthMs: 4 }],
      wowDepthMs: [0, 14, { wow: 1 }], flutterDepthMs: [0, 6, { wow: 1 }], wow: [0, 1, { wowDepthMs: 10, flutterDepthMs: 4 }],
      hiss: [0, 1, {}, "silence"], hum: [0, 1, {}, "silence"],
      dropout: [0, 1], dropoutMs: [8, 220, { dropout: 1 }],
      ceiling: [0.2, 1, {}, "loud"], outGain: [0, 1.5],
    },
  },
  television: {
    build: buildTelevisionGraph,
    defaults: { ...televisionDefaults(), static: 0, hum: 0, whine: 0, bedEnable: false },
    cases: {
      vibe: [0, 1], speaker: [0, 1], agc: [0, 1, {}, "loud"],
      hpHz: [20, 1200], lpHz: [800, 18000],
      midHumpDb: [-6, 10], midFreq: [600, 5000, { midHumpDb: 10 }],
      static: [0, 1, {}, "silence"], noiseHiss: [0, 1, { static: 1 }, "silence"], noiseCrackle: [0, 1, { static: 1 }, "silence"],
      hum: [0, 1, {}, "silence"], whine: [0, 1, {}, "silence"], outGain: [0, 1.5],
    },
  },
  occlusion: {
    build: buildOcclusionGraph,
    defaults: { ...occlusionDefaults(), rattle: 0 },
    cases: {
      distance: [0, 1], wall: [0, 1], sourceRoom: [0, 1], listenerRoom: [0, 1],
      hpHz: [10, 600], lpHz: [800, 18000],
      dipDb: [-12, 0], dipHz: [200, 5000, { dipDb: -12 }], dipQ: [0.2, 8, { dipDb: -12 }],
      bumpDb: [0, 10], bumpHz: [60, 2000, { bumpDb: 10 }], bumpQ: [0.2, 5, { bumpDb: 10 }],
      resonance: [0, 1], cavity: [0, 1], rattle: [0, 1], looseness: [0, 1, { rattle: 1 }],
      smear: [0, 1], leak: [0, 1], leakTone: [0, 1, { leak: 1 }],
      roomMix: [0, 1], predelayMs: [0, 28, { roomMix: 1 }], roomSize: [0, 1, { roomMix: 1 }], damp: [0, 1, { roomMix: 1 }],
      outGain: [0, 1.5],
    },
  },
  transmission: {
    build: (context, { seed: graphSeed, settings }) => buildTransmissionGraph(context, { seed: graphSeed, passes: settings.passes ?? 1 }),
    defaults: { ...transmissionDefaults(), walkieMode: false, tuningEnable: false, noiseProfile: 0, hiss: 0, crackle: 0, dropRate: 0 },
    cases: {
      hpHz: [40, 1200], lpHz: [1200, 12000], midGainDb: [-6, 8], midFreq: [600, 3500, { midGainDb: 8 }], midQ: [0.4, 5, { midGainDb: 8 }], boxDipDb: [0, 6],
      preDrive: [0, 1], postDrive: [0, 1], asym: [0, 1, { preDrive: 1, postDrive: 1 }], comp: [0, 1, {}, "loud"], crush: [0, 1],
      wowDepth: [0, 1, { badConnection: 1 }], dropRate: [0, 1, { badConnection: 1 }], dropDepth: [0, 1, { badConnection: 1, dropRate: 1 }], crackle: [0, 1, { badConnection: 1 }], lfoRate: [0.1, 3, { badConnection: 1, wowDepth: 1 }],
      noiseColor: [0, 1, { noiseProfile: 1, hiss: 1 }, "silence"], hiss: [0, 1, { noiseProfile: 1 }, "silence"], outGain: [0, 1.5], passes: [1, 6],
    },
  },
  comms: {
    build: buildCommsGraph,
    defaults: { ...commsDefaults(), hum: 0, hiss: 0, packet: 0, echoMix: 0, verbMix: 0, alarmTone: false },
    cases: {
      mode: ["landline", "pa"],
      hpHz: [80, 900], lpHz: [1200, 9000], midHumpDb: [0, 12], midFreq: [600, 5000, { midHumpDb: 12 }],
      drive: [0, 1], comp: [0, 1, {}, "loud"], bits: [6, 16], rate: [8000, 48000],
      packet: [0, 1], packetMs: [8, 120, { packet: 0.9 }], hum: [0, 1, {}, "silence"], hiss: [0, 1, {}, "silence"], toneMix: [0, 1, { alarmTone: true, mode: "alarm" }, "silence"],
      transducer: [0, 1], lineAge: [0, 1], duplex: [0, 1, { mode: "intercom" }, "quiet"], speakerRattle: [0, 1, { mode: "pa", transducer: 1 }], distance: [0, 1, { mode: "pa" }],
      ceiling: [0.2, 1, {}, "loud"], outGain: [0, 1.5],
      echoMix: [0, 1], echoMs: [25, 900, { echoMix: 1 }], echoFb: [0, 0.88, { echoMix: 1 }], echoTone: [0, 1, { echoMix: 1 }],
      verbMix: [0, 1], verbMs: [35, 1200, { verbMix: 1 }], verbDamp: [0, 1, { verbMix: 1 }],
    },
  },
  conference: {
    build: buildConferenceGraph,
    defaults: { ...conferenceDefaults(), packetLoss: 0, jitterMs: 0, gate: 0, noise: 0, robot: 0, bufferSlip: 0, bandwidthSwitch: 0 },
    cases: {
      hpHz: [80, 900], lpHz: [1200, 12000], midHumpDb: [0, 12], midFreq: [900, 4200, { midHumpDb: 12 }],
      mode: ["discord", "cell", { packetLoss: 0.45, noise: 0.2 }], concealMode: ["hold", "repeat", { packetLoss: 0.55 }], packetLoss: [0, 1], packetMs: [8, 180, { packetLoss: 0.55 }], repeatMs: [6, 240, { packetLoss: 0.55, concealMode: "repeat" }],
      jitterMs: [0, 12], jitterRate: [1, 80, { jitterMs: 12, bufferSlip: 0.6 }],
      burstiness: [0, 1, { packetLoss: 0.55 }], bufferSlip: [0, 1], bandwidthSwitch: [0, 1],
      suppression: [0, 1, { gate: 0.4 }, "quiet"], gate: [0, 1, {}, "quiet"], agc: [0, 1, {}, "quiet"], comfortNoise: [0, 1, { noise: 1 }, "silence"],
      bits: [4, 16], rate: [6000, 48000], robot: [0, 1, { packetLoss: 0.2, bufferSlip: 0.3 }], noise: [0, 1, { comfortNoise: 1 }, "silence"],
      ceiling: [0.2, 1, {}, "loud"], outGain: [0, 1.5],
    },
  },
  cartridge: {
    build: buildCartridgeGraph,
    defaults: { ...cartridgeDefaults(), bleepsEnable: false, dither: false, hum: 0, whine: 0, noise: 0, dcDrift: 0, microDelayMix: 0, verb: 0 },
    cases: {
      hpHz: [20, 240], lpHz: [2500, 18000], speaker: [0, 1], speakerModel: ["direct", "pc", { speaker: 1 }], bits: [2, 16], rate: [6000, 48000], jitter: [0, 1], dither: [false, true, { bits: 4 }], noiseShaping: [false, true, { bits: 4, dither: true }],
      preEmph: [0, 1], codecMode: ["pcm", "brr", { mulaw: 1, blockMs: 8 }], mulaw: [0, 1], blockMs: [0, 60, { codecMode: "brr", mulaw: 1 }], sat: [0, 1], edge: [0, 1], dcDrift: [0, 1], hum: [0, 1, {}, "silence"], whine: [0, 1, {}, "silence"], noise: [0, 1, {}, "silence"], noiseTrack: [0, 1, { noise: 1 }],
      microDelayMix: [0, 1], microDelayMs: [0, 30, { microDelayMix: 1 }], verb: [0, 1], verbMs: [10, 120, { verb: 1 }], limiter: [0, 1, { outGain: 1.5, ceiling: 0.2 }, "loud"], ceiling: [0.2, 1, {}, "loud"], wet: [0, 1], outGain: [0, 1.5],
      bleepsEnable: [false, true, { bleepsMix: 1, bleepsTrigger: "clock" }, "silence"], bleepsMix: [0, 1, { bleepsEnable: true, bleepsTrigger: "clock" }, "silence"], bleepsRate: [1, 18, { bleepsEnable: true, bleepsMix: 1, bleepsTrigger: "clock" }, "silence"], bleepsWave: ["pulse", "noise", { bleepsEnable: true, bleepsMix: 1, bleepsTrigger: "clock" }, "silence"], bleepsTrigger: ["transient", "clock", { bleepsEnable: true, bleepsMix: 1 }, "signal"], bleepsScale: ["minor", "major", { bleepsEnable: true, bleepsMix: 1, bleepsTrigger: "clock" }, "silence"], bleepsVibrato: [0, 1, { bleepsEnable: true, bleepsMix: 1, bleepsTrigger: "clock" }, "silence"], bleepsPitch: [0, 1, { bleepsEnable: true, bleepsMix: 1, bleepsTrigger: "clock" }, "silence"],
    },
  },
  cd: {
    build: buildCdGraph,
    defaults: { ...cdDefaults(), errorRate: 0, scratchRate: 0, trackingRate: 0, jitterMs: 0, servoNoise: 0, carComp: 0, softClip: false },
    cases: {
      mode: ["hold", "random", { errorRate: 1, correction: 0 }], damageShape: ["sine", "square", { scratchRate: 1, scratchAmt: 1, correction: 0 }], errorRate: [0, 1, { correction: 0 }], burstMs: [4, 260, { errorRate: 1, correction: 0 }], repeatMs: [6, 220, { errorRate: 1, correction: 0, mode: "repeat" }],
      scratchRate: [0, 1, { scratchAmt: 1, correction: 0 }], scratchAmt: [0, 1, { scratchRate: 1, correction: 0 }], correction: [0, 1, { errorRate: 1 }], interpolationMs: [0.25, 30, { errorRate: 1, scratchRate: 1, scratchAmt: 1, correction: 0, burstMs: 12 }], rotationHz: [2, 10, { scratchRate: 1, scratchAmt: 1, correction: 0 }],
      trackingRate: [0, 1, { correction: 0 }], trackingMs: [10, 1800, { trackingRate: 1, correction: 0 }], servoHunt: [0, 1, { errorRate: 1, correction: 0, servoNoise: 1 }, "silence"],
      jitterMs: [0, 1.25], jitterRate: [1, 200, { jitterMs: 1.25 }], hfLoss: [0, 1], servoNoise: [0, 1, { errorRate: 1, correction: 1 }, "silence"],
      carComp: [0, 1, {}, "loud"], softClip: [false, true, {}, "loud"], ceiling: [0.2, 1, {}, "loud"], outGain: [0, 1.2],
      __triggerDamage: [false, true, { mode: "random", burstMs: 120 }], __triggerSkip: [false, true, { repeatMs: 42, trackingMs: 260 }],
    },
  },
  camcorder: {
    build: buildCamcorderGraph,
    defaults: { ...camcorderDefaults(), wind: false, handling: 0, rub: 0, hiss: 0, drop: 0, chirp: 0 },
    cases: {
      format: ["vhsc", "action", { flutter: 0.7, corruption: 0.55 }], micModel: ["cheapMono", "shotgun"],
      hpHz: [10, 280], lpHz: [1400, 18000], boxDb: [0, 12], boxHz: [650, 3200, { boxDb: 12 }],
      coverage: [0, 1], movement: [0, 1, { handling: 0.7, rub: 0.5 }], corruption: [0, 1], agc: [0, 1, {}, "loud"], agcAmt: [0, 1, {}, "loud"], agcSpeed: [0, 1, { agcAmt: 1 }, "loud"], agcPump: [0, 1, { agcAmt: 1, hiss: 0.7 }, "quiet"], clip: [0, 1, {}, "loud"],
      crush: [0, 1], bits: [6, 16], rate: [8000, 48000], flutter: [0, 1, { format: "vhsc", corruption: 0.5 }], drop: [0, 1], dropMs: [6, 260, { drop: 1 }], dropMode: ["hold", "repeat", { drop: 1 }], repeatMs: [8, 240, { drop: 1, dropMode: "repeat" }], chirp: [0, 1, { corruption: 1 }], handling: [0, 1], rub: [0, 1], hiss: [0, 1, {}, "silence"], motorBleed: [0, 1, {}, "silence"], ceiling: [0.2, 1, {}, "loud"], outGain: [0, 1.5],
    },
  },
};

const results = {};
const query = new URLSearchParams(location.search);
const requestedGroup = query.get("group") || "analog";
const requestedEngine = query.get("engine");
const groupNames = requestedEngine && engines[requestedEngine] ? [requestedEngine] : requestedGroup === "digital"
  ? ["transmission", "comms", "conference", "cartridge", "cd", "camcorder"]
  : ["tape", "television", "occlusion"];
for (const engineName of groupNames) {
  const engine = engines[engineName];
  results[engineName] = {};
  for (const [parameter, spec] of Object.entries(engine.cases)) {
    const [low, high, setup = {}, kind = "signal"] = spec;
    const base = { ...engine.defaults, ...setup };
    const lowOutput = await render(engine, { ...base, [parameter]: low }, kind);
    const highOutput = await render(engine, { ...base, [parameter]: high }, kind);
    results[engineName][parameter] = compare(lowOutput, highOutput);
    document.querySelector("#results").textContent = JSON.stringify(results, null, 2);
  }
}

document.body.dataset.complete = "true";
