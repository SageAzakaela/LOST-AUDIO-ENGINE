import { PRESETS as TRANSMISSION_PRESETS } from "../../src/presets.js";
import { PRESETS as OCCLUSION_PRESETS } from "../../occlusion-engine/src/presets.js?v=20260827.18";
import { PRESETS as COMMS_PRESETS } from "../../comms-engine/src/presets.js?v=20260827.26";
import { PRESETS as CONFERENCE_PRESETS } from "../../conference-engine/src/presets.js?v=20260827.24";
import { PRESETS as TAPE_PRESETS } from "../../tape-engine/src/presets.js?v=20260827.21";
import { PRESETS as TV_PRESETS } from "../../television-engine/src/presets.js";
import { PRESETS as CARTRIDGE_PRESETS } from "../../cartridge-engine/src/presets.js?v=20260827.26";
import { PRESETS as CD_PRESETS } from "../../cd-engine/src/presets.js?v=20260827.3";
import { PRESETS as CAMCORDER_PRESETS } from "../../camcorder-engine/src/presets.js?v=20260827.27";

export const ENGINE_PRESETS = {
  occlusion: OCCLUSION_PRESETS,
  transmission: TRANSMISSION_PRESETS,
  comms: COMMS_PRESETS,
  conference: CONFERENCE_PRESETS,
  tape: TAPE_PRESETS,
  television: TV_PRESETS,
  cartridge: CARTRIDGE_PRESETS,
  cd: CD_PRESETS,
  camcorder: CAMCORDER_PRESETS,
};

export const MASTER_PRESETS = [
  {
    id: "safety-clean",
    name: "Safety Clean",
    desc: "All modules off (safe starting point).",
    master: { masterGain: 0.25, masterHpHz: 20, masterLpHz: 20000, masterComp: 0.12, ceiling: 0.92, limiter: 0.45, softClip: true, monoOut: false },
    order: ["occlusion", "transmission", "comms", "tape", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      occlusion: { enabled: false, wet: 1, params: {} },
      transmission: { enabled: false, wet: 1, params: {} },
      comms: { enabled: false, wet: 1, params: {} },
      tape: { enabled: false, wet: 1, params: {} },
      television: { enabled: false, wet: 1, params: {} },
      cartridge: { enabled: false, wet: 1, params: {} },
      cd: { enabled: false, wet: 1, params: {} },
      camcorder: { enabled: false, wet: 1, params: {} },
      conference: { enabled: false, wet: 1, params: {} },
    },
  },
  {
    id: "subtle-am",
    name: "Subtle AM",
    desc: "Light AM narrowing (no jump scares).",
    master: { masterGain: 0.25, masterHpHz: 20, masterLpHz: 20000, masterComp: 0.12, ceiling: 0.92, limiter: 0.45, softClip: true, monoOut: true },
    order: ["occlusion", "transmission", "comms", "tape", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      transmission: { enabled: true, wet: 0.35, params: { ...TRANSMISSION_PRESETS.portableAmSoft, tuningEnable: false, passes: 1, outGain: 0.92 } },
    },
  },
  {
    id: "next-room",
    name: "Next Room",
    desc: "Occluded behind a wall with room reflections.",
    master: { masterGain: 0.25, masterHpHz: 20, masterLpHz: 20000, masterComp: 0.12, ceiling: 0.92, limiter: 0.45, softClip: true, monoOut: false },
    order: ["occlusion", "transmission", "comms", "tape", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      occlusion: { enabled: true, wet: 0.75, params: { ...OCCLUSION_PRESETS.nextRoom } },
    },
  },
  {
    id: "warm-landline",
    name: "Warm Landline",
    desc: "Light phone coloration with tiny room.",
    master: { masterGain: 0.25, masterHpHz: 20, masterLpHz: 20000, masterComp: 0.12, ceiling: 0.92, limiter: 0.45, softClip: true, monoOut: true },
    order: ["occlusion", "transmission", "comms", "tape", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      comms: { enabled: true, wet: 0.45, params: { ...COMMS_PRESETS.landlineWarm, echoMix: 0.02, verbMix: 0.03 } },
    },
  },
  {
    id: "light-vhs",
    name: "Light VHS",
    desc: "Realistic VHS wear with an audible transport bed.",
    master: { masterGain: 0.25, masterHpHz: 20, masterLpHz: 20000, masterComp: 0.12, ceiling: 0.92, limiter: 0.45, softClip: true, monoOut: false },
    order: ["occlusion", "transmission", "comms", "tape", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      tape: { enabled: true, wet: 0.45, params: { ...TAPE_PRESETS.vhsRental } },
    },
  },
  {
    id: "living-room-crt",
    name: "Living Room CRT",
    desc: "TV speaker, flyback character, and a gentle CRT room bed.",
    master: { masterGain: 0.25, masterHpHz: 20, masterLpHz: 20000, masterComp: 0.12, ceiling: 0.92, limiter: 0.45, softClip: true, monoOut: false },
    order: ["occlusion", "transmission", "comms", "tape", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      television: { enabled: true, wet: 0.58, params: { ...TV_PRESETS.livingRoomCrt, outGain: 1 } },
    },
  },
  {
    id: "chewed-cassette",
    name: "Chewed Cassette",
    desc: "Exaggerated transport, pitch instability, saturation, and damaged tape movement.",
    master: { masterGain: 0.25, masterHpHz: 20, masterLpHz: 20000, masterComp: 0.08, ceiling: 0.88, limiter: 0.58, softClip: true, monoOut: false },
    order: ["occlusion", "transmission", "comms", "tape", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      tape: { enabled: true, wet: 0.82, params: { ...TAPE_PRESETS.chewedTapeExaggerated } },
    },
  },
  {
    id: "lost-broadcast",
    name: "Lost Broadcast",
    desc: "Exaggerated CRT receiver with prominent static, cabinet bed, hum, and audible flyback instability.",
    master: { masterGain: 0.25, masterHpHz: 20, masterLpHz: 18000, masterComp: 0.08, ceiling: 0.86, limiter: 0.62, softClip: true, monoOut: true },
    order: ["occlusion", "transmission", "comms", "tape", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      television: { enabled: true, wet: 0.86, params: { ...TV_PRESETS.lostBroadcastExaggerated } },
    },
  },
  {
    id: "handheld-clean",
    name: "Handheld Clean",
    desc: "Subtle retro cartridge-ish compression.",
    master: { masterGain: 0.25, masterHpHz: 20, masterLpHz: 20000, masterComp: 0.12, ceiling: 0.92, limiter: 0.45, softClip: true, monoOut: true },
    order: ["occlusion", "transmission", "comms", "tape", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      cartridge: { enabled: true, wet: 0.35, params: { ...CARTRIDGE_PRESETS.handheldClean, bleepsEnable: false } },
    },
  },
  {
    id: "light-dust-cd",
    name: "Light Dust CD",
    desc: "A little CD wear without big glitches.",
    master: { masterGain: 0.25, masterHpHz: 20, masterLpHz: 20000, masterComp: 0.12, ceiling: 0.92, limiter: 0.45, softClip: true, monoOut: false },
    order: ["occlusion", "transmission", "comms", "tape", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      cd: { enabled: true, wet: 0.4, params: { ...CD_PRESETS.lightDust } },
    },
  },
  {
    id: "steady-hand-cam",
    name: "Steady Hand Cam",
    desc: "Light camcorder color; minimal wind/movement.",
    master: { masterGain: 0.25, masterHpHz: 20, masterLpHz: 20000, masterComp: 0.12, ceiling: 0.92, limiter: 0.45, softClip: true, monoOut: true },
    order: ["occlusion", "transmission", "comms", "tape", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      camcorder: { enabled: true, wet: 0.45, params: { ...CAMCORDER_PRESETS.steadyHand, wind: false } },
    },
  },
  {
    id: "clean-call",
    name: "Clean Call",
    desc: "Light conference/call coloration.",
    master: { masterGain: 0.25, masterHpHz: 20, masterLpHz: 20000, masterComp: 0.12, ceiling: 0.92, limiter: 0.45, softClip: true, monoOut: true },
    order: ["occlusion", "transmission", "comms", "tape", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      conference: { enabled: true, wet: 0.4, params: { ...CONFERENCE_PRESETS.cleanCall } },
    },
  },
  {
    id: "apartment-tv-next-door",
    name: "Apartment TV Next Door",
    desc: "A late-night CRT heard through the neighboring apartment wall.",
    master: { inputTrim: 0.45, masterGain: 0.25, masterHpHz: 28, masterLpHz: 18000, masterComp: 0.16, ceiling: 0.9, limiter: 0.56, softClip: true, monoOut: false },
    order: ["television", "occlusion", "transmission", "comms", "tape", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      television: { enabled: true, wet: 0.52, params: { ...TV_PRESETS.lateNightBroadcast, bedEnable: false, bedLevel: 0, bedSource: "" } },
      occlusion: { enabled: true, wet: 0.72, params: { ...OCCLUSION_PRESETS.nextRoom, rattle: 0.025, outGain: 0.96 } },
    },
  },
  {
    id: "motel-answering-machine",
    name: "Motel Answering Machine",
    desc: "A microcassette message played through a worn motel telephone receiver.",
    master: { inputTrim: 0.44, masterGain: 0.25, masterHpHz: 34, masterLpHz: 15000, masterComp: 0.18, ceiling: 0.89, limiter: 0.6, softClip: true, monoOut: true },
    order: ["tape", "comms", "occlusion", "transmission", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      tape: { enabled: true, wet: 0.62, params: { ...TAPE_PRESETS.dictaphone, sfxEnable: false, sfxLevel: 0, hiss: 0.16, hum: 0.055 } },
      comms: { enabled: true, wet: 0.54, params: { ...COMMS_PRESETS.motelPhoneReceiver, noise: 0.16, speakerRattle: 0.24 } },
    },
  },
  {
    id: "webcam-call-2008",
    name: "2008 Webcam Call",
    desc: "Cheap desktop camera capture passed through an unstable old video call.",
    master: { inputTrim: 0.44, masterGain: 0.25, masterHpHz: 30, masterLpHz: 17000, masterComp: 0.16, ceiling: 0.89, limiter: 0.6, softClip: true, monoOut: true },
    order: ["camcorder", "conference", "occlusion", "transmission", "comms", "tape", "television", "cartridge", "cd"],
    modules: {
      camcorder: { enabled: true, wet: 0.56, params: { ...CAMCORDER_PRESETS.cheapDigicam, wind: false, windBedSource: "", windHitSource: "", camBedSource: "", chirp: 0.045, handling: 0.06, rub: 0.04 } },
      conference: { enabled: true, wet: 0.66, params: { ...CONFERENCE_PRESETS.skype2008, robot: 0.24, dropouts: 0.3 } },
    },
  },
  {
    id: "mall-security-archive",
    name: "Mall Security Archive",
    desc: "A fixed consumer camera recording preserved on a worn surveillance VHS.",
    master: { inputTrim: 0.42, masterGain: 0.25, masterHpHz: 36, masterLpHz: 16000, masterComp: 0.17, ceiling: 0.88, limiter: 0.62, softClip: true, monoOut: true },
    order: ["camcorder", "tape", "occlusion", "transmission", "comms", "television", "cartridge", "cd", "conference"],
    modules: {
      camcorder: { enabled: true, wet: 0.64, params: { ...CAMCORDER_PRESETS.foundFootage, wind: false, windBedSource: "", windHitSource: "", camBedSource: "", chirp: 0.07, handling: 0.07, rub: 0.04, motorBleed: 0.14 } },
      tape: { enabled: true, wet: 0.46, params: { ...TAPE_PRESETS.vhsLinear, sfxEnable: false, sfxLevel: 0, hiss: 0.11, hum: 0.035 } },
    },
  },
  {
    id: "radio-in-the-vent",
    name: "Radio in the Vent",
    desc: "A weak portable broadcast resonating through loose metal ventilation.",
    master: { inputTrim: 0.42, masterGain: 0.25, masterHpHz: 38, masterLpHz: 15000, masterComp: 0.14, ceiling: 0.88, limiter: 0.64, softClip: true, monoOut: true },
    order: ["transmission", "occlusion", "comms", "tape", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      transmission: { enabled: true, wet: 0.62, params: { ...TRANSMISSION_PRESETS.weakSignal, tuningEnable: false, tuningAmount: 0, passes: 1, hiss: 0.24, crackle: 0.2, outGain: 0.88 } },
      occlusion: { enabled: true, wet: 0.48, params: { ...OCCLUSION_PRESETS.looseSheetMetal, rattle: 0.52, looseness: 0.68, resonance: 0.76, outGain: 0.86 } },
    },
  },
  {
    id: "bootleg-game-cutscene",
    name: "Bootleg Game Cutscene",
    desc: "Compressed console dialogue emerging from a small, tired CRT television.",
    master: { inputTrim: 0.45, masterGain: 0.25, masterHpHz: 30, masterLpHz: 17000, masterComp: 0.16, ceiling: 0.89, limiter: 0.58, softClip: true, monoOut: true },
    order: ["cartridge", "television", "occlusion", "transmission", "comms", "tape", "cd", "camcorder", "conference"],
    modules: {
      cartridge: { enabled: true, wet: 0.66, params: { ...CARTRIDGE_PRESETS.cutsceneADPCM, bleepsEnable: false, bleepsMix: 0, noise: 0.014, verb: 0.16 } },
      television: { enabled: true, wet: 0.5, params: { ...TV_PRESETS.smallKitchenTv, bedEnable: false, bedLevel: 0, bedSource: "", static: 0.09, hum: 0.08, whine: 0.04 } },
    },
  },
  {
    id: "scratched-disc-over-discord",
    name: "Scratched Disc Over Discord",
    desc: "A damaged CD rip shared through a call that keeps losing the voice frames.",
    master: { inputTrim: 0.42, masterGain: 0.25, masterHpHz: 26, masterLpHz: 18000, masterComp: 0.14, ceiling: 0.87, limiter: 0.66, softClip: true, monoOut: true },
    order: ["cd", "conference", "occlusion", "transmission", "comms", "tape", "television", "cartridge", "camcorder"],
    modules: {
      cd: { enabled: true, wet: 0.58, params: { ...CD_PRESETS.oneBadRip, errorRate: 0.2, scratchRate: 0.2, scratchAmt: 0.36, correction: 0.68, servoNoise: 0.07 } },
      conference: { enabled: true, wet: 0.64, params: { ...CONFERENCE_PRESETS.discordCrunch, dropouts: 0.42, robot: 0.48, noise: 0.035 } },
    },
  },
  {
    id: "hospital-lockdown-pa",
    name: "Hospital Lockdown PA",
    desc: "A distant institutional announcement reflecting through concrete corridors.",
    master: { inputTrim: 0.45, masterGain: 0.25, masterHpHz: 34, masterLpHz: 18000, masterComp: 0.18, ceiling: 0.9, limiter: 0.58, softClip: true, monoOut: false },
    order: ["comms", "occlusion", "transmission", "tape", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      comms: { enabled: true, wet: 0.72, params: { ...COMMS_PRESETS.hospitalCorridorPa, noise: 0.045, speakerRattle: 0.42, verbMix: 0.2 } },
      occlusion: { enabled: true, wet: 0.34, params: { ...OCCLUSION_PRESETS.concreteBunker, wall: 0.58, distance: 0.5, leak: 0.035, roomMix: 0.24, outGain: 0.98 } },
    },
  },
  {
    id: "ghost-vhs-screening",
    name: "Ghost VHS Screening",
    desc: "An unstable rental tape playing through a fading portable television.",
    master: { inputTrim: 0.4, masterGain: 0.25, masterHpHz: 32, masterLpHz: 16000, masterComp: 0.12, ceiling: 0.86, limiter: 0.68, softClip: true, monoOut: true },
    order: ["tape", "television", "occlusion", "transmission", "comms", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      tape: { enabled: true, wet: 0.56, params: { ...TAPE_PRESETS.ghostVhsExaggerated, sfxEnable: false, sfxLevel: 0, hiss: 0.18, hum: 0.06, dropout: 0.36, outGain: 0.96 } },
      television: { enabled: true, wet: 0.5, params: { ...TV_PRESETS.antennaSnowSoft, bedEnable: false, bedLevel: 0, bedSource: "", static: 0.24, hum: 0.08, whine: 0.04 } },
    },
  },
  {
    id: "pirate-station-cassette",
    name: "Pirate Station Cassette",
    desc: "A home-recorded cassette rebroadcast through a small unauthorized AM transmitter.",
    master: { inputTrim: 0.44, masterGain: 0.25, masterHpHz: 30, masterLpHz: 17000, masterComp: 0.16, ceiling: 0.89, limiter: 0.6, softClip: true, monoOut: true },
    order: ["tape", "transmission", "occlusion", "comms", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      tape: { enabled: true, wet: 0.52, params: { ...TAPE_PRESETS.cassetteHomeDemo, sfxEnable: false, sfxLevel: 0, hiss: 0.13, hum: 0.04 } },
      transmission: { enabled: true, wet: 0.58, params: { ...TRANSMISSION_PRESETS.portableAmSoft, tuningEnable: false, tuningAmount: 0, passes: 1, noiseProfile: 0.08, hiss: 0.12, outGain: 0.92 } },
    },
  },
];
