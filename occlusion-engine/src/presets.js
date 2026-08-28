export const PRESETS = {
  subtleRoom: {
    distance: 0.2, wall: 0.18, material: "drywall", construction: "stud",
    sourceRoom: 0.22, listenerRoom: 0.3,
    resonance: 0.28, cavity: 0.3, rattle: 0.015, looseness: 0.2, smear: 0.18,
    leak: 0.1, leakTone: 0.58, roomMix: 0.12, predelayMs: 5, outGain: 1,
  },
  nextRoom: {
    distance: 0.45, wall: 0.55, material: "drywall", construction: "stud",
    sourceRoom: 0.35, listenerRoom: 0.52,
    resonance: 0.55, cavity: 0.58, rattle: 0.06, looseness: 0.34, smear: 0.45,
    leak: 0.06, leakTone: 0.5, roomMix: 0.22, predelayMs: 9, outGain: 1,
  },
  behindDoor: {
    distance: 0.38, wall: 0.58, material: "door", construction: "hollow",
    sourceRoom: 0.32, listenerRoom: 0.42,
    resonance: 0.66, cavity: 0.76, rattle: 0.16, looseness: 0.48, smear: 0.52,
    leak: 0.13, leakTone: 0.64, roomMix: 0.16, predelayMs: 6, outGain: 1,
  },
  brickMuffle: {
    distance: 0.55, wall: 0.78, material: "brick", construction: "solid",
    sourceRoom: 0.32, listenerRoom: 0.48,
    resonance: 0.23, cavity: 0.015, rattle: 0, looseness: 0.04, smear: 0.18,
    leak: 0.025, leakTone: 0.2, roomMix: 0.2, predelayMs: 10, outGain: 1.04,
  },
  curtainLeak: {
    distance: 0.32, wall: 0.2, material: "curtain", construction: "panel",
    sourceRoom: 0.28, listenerRoom: 0.5,
    resonance: 0.12, cavity: 0.04, rattle: 0, looseness: 0.08, smear: 0.1,
    leak: 0.24, leakTone: 0.72, roomMix: 0.2, predelayMs: 10, outGain: 1,
  },
  hollowStudWall: {
    distance: 0.48, wall: 0.48, material: "drywall", construction: "hollow",
    sourceRoom: 0.3, listenerRoom: 0.4,
    resonance: 0.72, cavity: 0.84, rattle: 0.14, looseness: 0.52, smear: 0.58,
    leak: 0.05, leakTone: 0.48, roomMix: 0.18, predelayMs: 7, outGain: 0.98,
  },
  woodPanelRattle: {
    distance: 0.35, wall: 0.4, material: "wood", construction: "panel",
    sourceRoom: 0.22, listenerRoom: 0.34,
    resonance: 0.78, cavity: 0.44, rattle: 0.52, looseness: 0.7, smear: 0.6,
    leak: 0.08, leakTone: 0.62, roomMix: 0.14, predelayMs: 5, outGain: 0.96,
  },
  glassPartition: {
    distance: 0.38, wall: 0.32, material: "glass", construction: "panel",
    sourceRoom: 0.3, listenerRoom: 0.38,
    resonance: 0.75, cavity: 0.25, rattle: 0.22, looseness: 0.45, smear: 0.46,
    leak: 0.08, leakTone: 0.84, roomMix: 0.15, predelayMs: 6, outGain: 0.96,
  },
  concreteBunker: {
    distance: 0.62, wall: 0.92, material: "concrete", construction: "solid",
    sourceRoom: 0.44, listenerRoom: 0.5,
    resonance: 0.16, cavity: 0.01, rattle: 0, looseness: 0, smear: 0.12,
    leak: 0.008, leakTone: 0.12, roomMix: 0.18, predelayMs: 11, outGain: 1.08,
  },
  looseSheetMetal: {
    distance: 0.3, wall: 0.36, material: "metal", construction: "loose",
    sourceRoom: 0.18, listenerRoom: 0.28,
    resonance: 0.9, cavity: 0.68, rattle: 0.88, looseness: 0.94, smear: 0.78,
    leak: 0.07, leakTone: 0.78, roomMix: 0.12, predelayMs: 4, outGain: 0.88,
  },
};
