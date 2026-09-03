import assert from "node:assert/strict";
import {
  MOBILE_REALTIME_RELEASE_MS,
  MOBILE_VISUAL_FRAME_MS,
  realtimePreviewPolicy,
  shouldUpdateMasterMeter,
  shouldUpdateRackMeters,
} from "../lame/src/mobile-power.js";

const highRateMobile = realtimePreviewPolicy({ mobile: true, sourceSampleRate: 96000 });
assert.deepEqual(highRateMobile, { sampleRate: 48000, latencyHint: "playback", analyserFftSize: 1024 });

const standardMobile = realtimePreviewPolicy({ mobile: true, sourceSampleRate: 44100 });
assert.equal(standardMobile.sampleRate, 44100, "normal-rate sources should not be upsampled or downsampled");

const desktop = realtimePreviewPolicy({ mobile: false, sourceSampleRate: 96000 });
assert.deepEqual(desktop, { sampleRate: 96000, latencyHint: "interactive", analyserFftSize: 2048 });

assert.equal(Math.round(1000 / MOBILE_VISUAL_FRAME_MS), 24);
assert.equal(MOBILE_REALTIME_RELEASE_MS, 2500);
assert.equal(shouldUpdateRackMeters({ mobile: true, panel: "rack" }), true);
assert.equal(shouldUpdateRackMeters({ mobile: true, panel: "output" }), false);
assert.equal(shouldUpdateMasterMeter({ mobile: true, panel: "output" }), true);
assert.equal(shouldUpdateMasterMeter({ mobile: true, panel: "rack" }), false);
assert.equal(shouldUpdateRackMeters({ mobile: false, panel: "output" }), true);
assert.equal(shouldUpdateMasterMeter({ mobile: false, panel: "rack" }), true);

console.log(JSON.stringify({ mobilePreviewHz: highRateMobile.sampleRate, mobileVisualFps: Math.round(1000 / MOBILE_VISUAL_FRAME_MS), idleReleaseMs: MOBILE_REALTIME_RELEASE_MS }));
