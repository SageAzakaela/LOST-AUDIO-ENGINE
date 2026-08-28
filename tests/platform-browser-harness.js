import { detectPlatformCapabilities } from "../lame/src/platform.js?v=20260827.1";

const report = detectPlatformCapabilities(window);
document.querySelector("#results").textContent = JSON.stringify(report, null, 2);
document.body.dataset.complete = "true";
