import assert from "node:assert/strict";
import { detectPlatformCapabilities, formatPlatformReport, getAudioContextConstructor, getOfflineAudioContextConstructor } from "../lame/src/platform.js";

class RealtimeContext {}
class OfflineContext {}
class WorkletNode {}
class PointerEvent {}
class File {}
class Blob {}

function scope(overrides = {}) {
  return {
    location: { protocol: "https:", hostname: "example.com" },
    isSecureContext: true,
    AudioContext: RealtimeContext,
    OfflineAudioContext: OfflineContext,
    AudioWorkletNode: WorkletNode,
    PointerEvent,
    File,
    Blob,
    URL: { createObjectURL() {} },
    requestAnimationFrame() {},
    navigator: { userAgent: "test-agent", platform: "test-platform" },
    document: {
      createElement(tag) {
        if (tag === "canvas") return { getContext: () => ({}) };
        if (tag === "a") return { download: "" };
        return {};
      },
    },
    ...overrides,
  };
}

const supported = detectPlatformCapabilities(scope());
assert.equal(supported.supported, true);
assert.equal(supported.missing.length, 0);
assert.match(formatPlatformReport(supported), /PASS  audio-worklet/);

const localHttp = detectPlatformCapabilities(scope({ isSecureContext: false, location: { protocol: "http:", hostname: "localhost" } }));
assert.equal(localHttp.supported, true, "localhost is a valid AudioWorklet development context");

const insecure = detectPlatformCapabilities(scope({ isSecureContext: false, location: { protocol: "http:", hostname: "example.com" } }));
assert.equal(insecure.supported, false);
assert.deepEqual(insecure.missing.map((item) => item.id), ["secure-context"]);

const incomplete = detectPlatformCapabilities(scope({ AudioWorkletNode: undefined, OfflineAudioContext: undefined, PointerEvent: undefined }));
assert.deepEqual(incomplete.missing.map((item) => item.id), ["offline-audio", "audio-worklet", "pointer-events"]);

const prefixed = scope({ AudioContext: undefined, OfflineAudioContext: undefined, webkitAudioContext: RealtimeContext, webkitOfflineAudioContext: OfflineContext });
assert.equal(getAudioContextConstructor(prefixed), RealtimeContext);
assert.equal(getOfflineAudioContextConstructor(prefixed), OfflineContext);

console.log(JSON.stringify({ supportedChecks: supported.checks.length, insecureMissing: insecure.missing.length, incompleteMissing: incomplete.missing.length }));
