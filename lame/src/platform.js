const LOCAL_HOSTS = new Set(["localhost", "127.0.0.1", "::1", "[::1]"]);

function available(value) {
  return typeof value !== "undefined" && value !== null;
}

export function getAudioContextConstructor(scope = globalThis) {
  return scope.AudioContext || scope.webkitAudioContext || null;
}

export function getOfflineAudioContextConstructor(scope = globalThis) {
  return scope.OfflineAudioContext || scope.webkitOfflineAudioContext || null;
}

export function detectPlatformCapabilities(scope = globalThis) {
  const location = scope.location || { protocol: "", hostname: "" };
  const hostname = String(location.hostname || "").toLowerCase();
  const secureContext = scope.isSecureContext === true || LOCAL_HOSTS.has(hostname);
  const document = scope.document;
  const canvas = document?.createElement?.("canvas");
  const anchor = document?.createElement?.("a");
  const checks = [
    { id: "secure-context", label: "HTTPS or localhost secure context", ok: secureContext && location.protocol !== "file:" },
    { id: "audio-context", label: "Web Audio realtime context", ok: Boolean(getAudioContextConstructor(scope)) },
    { id: "offline-audio", label: "OfflineAudioContext export rendering", ok: Boolean(getOfflineAudioContextConstructor(scope)) },
    { id: "audio-worklet", label: "AudioWorklet processing", ok: available(scope.AudioWorkletNode) },
    { id: "file-api", label: "Local File and Blob APIs", ok: available(scope.File) && available(scope.Blob) && typeof scope.URL?.createObjectURL === "function" },
    { id: "download", label: "Local WAV download", ok: Boolean(anchor && "download" in anchor) },
    { id: "canvas", label: "Canvas waveform rendering", ok: Boolean(canvas?.getContext?.("2d")) },
    { id: "pointer-events", label: "Pointer Events rack and automation input", ok: available(scope.PointerEvent) },
    { id: "animation-frame", label: "Animation frame scheduling", ok: typeof scope.requestAnimationFrame === "function" },
  ];
  const missing = checks.filter((check) => !check.ok);
  return {
    supported: missing.length === 0,
    checks,
    missing,
    hostname,
    protocol: String(location.protocol || ""),
    userAgent: String(scope.navigator?.userAgent || "unknown"),
    platform: String(scope.navigator?.userAgentData?.platform || scope.navigator?.platform || "unknown"),
  };
}

export function formatPlatformReport(report) {
  const rows = report.checks.map((check) => `${check.ok ? "PASS" : "MISS"}  ${check.id}`).join("\n");
  return [
    `platform: ${report.platform}`,
    `host: ${report.protocol}//${report.hostname}`,
    `user agent: ${report.userAgent}`,
    rows,
  ].join("\n");
}
