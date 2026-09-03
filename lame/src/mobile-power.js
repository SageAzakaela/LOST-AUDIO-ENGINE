export const MOBILE_VISUAL_FRAME_MS = 1000 / 24;
export const MOBILE_REALTIME_RELEASE_MS = 2500;

export function realtimePreviewPolicy({ mobile = false, sourceSampleRate = 44100 } = {}) {
  const sourceRate = Math.max(8000, Number(sourceSampleRate) || 44100);
  return {
    sampleRate: mobile ? Math.min(48000, sourceRate) : sourceRate,
    latencyHint: mobile ? "playback" : "interactive",
    analyserFftSize: mobile ? 1024 : 2048,
  };
}

export function shouldUpdateRackMeters({ mobile = false, panel = "rack" } = {}) {
  return !mobile || panel === "rack";
}

export function shouldUpdateMasterMeter({ mobile = false, panel = "rack" } = {}) {
  return !mobile || panel === "output";
}
