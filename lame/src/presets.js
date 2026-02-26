import { PRESETS as TRANSMISSION_PRESETS } from "../../src/presets.js";
import { PRESETS as OCCLUSION_PRESETS } from "../../occlusion-engine/src/presets.js";
import { PRESETS as COMMS_PRESETS } from "../../comms-engine/src/presets.js";
import { PRESETS as CONFERENCE_PRESETS } from "../../conference-engine/src/presets.js";
import { PRESETS as TAPE_PRESETS } from "../../tape-engine/src/presets.js";
import { PRESETS as TV_PRESETS } from "../../television-engine/src/presets.js";
import { PRESETS as CARTRIDGE_PRESETS } from "../../cartridge-engine/src/presets.js";
import { PRESETS as CD_PRESETS } from "../../cd-engine/src/presets.js";
import { PRESETS as CAMCORDER_PRESETS } from "../../camcorder-engine/src/presets.js";

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
    desc: "A touch of VHS/tape wear.",
    master: { masterGain: 0.25, masterHpHz: 20, masterLpHz: 20000, masterComp: 0.12, ceiling: 0.92, limiter: 0.45, softClip: true, monoOut: false },
    order: ["occlusion", "transmission", "comms", "tape", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      tape: { enabled: true, wet: 0.45, params: { ...TAPE_PRESETS.vhsRental } },
    },
  },
  {
    id: "living-room-crt",
    name: "Living Room CRT",
    desc: "TV speaker + gentle CRT noise.",
    master: { masterGain: 0.25, masterHpHz: 20, masterLpHz: 20000, masterComp: 0.12, ceiling: 0.92, limiter: 0.45, softClip: true, monoOut: false },
    order: ["occlusion", "transmission", "comms", "tape", "television", "cartridge", "cd", "camcorder", "conference"],
    modules: {
      television: { enabled: true, wet: 0.5, params: { ...TV_PRESETS.livingRoomCrt, bedEnable: false, outGain: 1 } },
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
];
