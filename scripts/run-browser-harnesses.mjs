import { spawn } from "node:child_process";
import { createServer } from "node:http";
import { existsSync, mkdirSync, mkdtempSync, readFile, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, extname, join, relative, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const outputPath = resolve(root, ".artifacts", "platform-browser-results.json");
const full = process.argv.includes("--full") || process.argv.includes("--ci");
const mimeTypes = new Map([
  [".css", "text/css; charset=utf-8"], [".html", "text/html; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"], [".json", "application/json; charset=utf-8"],
  [".mjs", "text/javascript; charset=utf-8"], [".mp3", "audio/mpeg"], [".wav", "audio/wav"],
]);

function browserExecutable() {
  if (process.env.LAE_BROWSER) return process.env.LAE_BROWSER;
  const candidates = process.platform === "win32" ? [
    join(process.env.ProgramFiles || "", "Google", "Chrome", "Application", "chrome.exe"),
    join(process.env["ProgramFiles(x86)"] || "", "Microsoft", "Edge", "Application", "msedge.exe"),
  ] : process.platform === "darwin" ? [
    "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
    "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge",
  ] : [
    "/usr/bin/google-chrome", "/usr/bin/google-chrome-stable", "/usr/bin/chromium", "/usr/bin/chromium-browser",
  ];
  return candidates.find((candidate) => candidate && existsSync(candidate)) || null;
}

function decodeEntities(text) {
  return text
    .replace(/&quot;/g, '"').replace(/&#39;/g, "'").replace(/&lt;/g, "<")
    .replace(/&gt;/g, ">").replace(/&amp;/g, "&")
    .replace(/&#(\d+);/g, (_, code) => String.fromCodePoint(Number(code)))
    .replace(/&#x([0-9a-f]+);/gi, (_, code) => String.fromCodePoint(Number.parseInt(code, 16)));
}

function collectSafetyFailures(value, path = "result", failures = []) {
  if (Array.isArray(value)) {
    value.forEach((entry, index) => collectSafetyFailures(entry, `${path}[${index}]`, failures));
    return failures;
  }
  if (!value || typeof value !== "object") return failures;
  for (const [key, entry] of Object.entries(value)) {
    const itemPath = `${path}.${key}`;
    if ((key === "nonFinite" || key === "nonFiniteSamples") && entry !== 0) failures.push(`${itemPath}=${entry}`);
    if (key === "ceilingSafe" && entry !== true) failures.push(`${itemPath}=${entry}`);
    if (key === "missing" && entry === true) failures.push(`${itemPath}=true`);
    if (key === "classification" && entry !== "ACTIVE") failures.push(`${itemPath}=${entry}`);
    if (typeof entry === "number" && !Number.isFinite(entry)) failures.push(`${itemPath}=non-finite`);
    collectSafetyFailures(entry, itemPath, failures);
  }
  return failures;
}

function staticServer() {
  return createServer((request, response) => {
    let pathname;
    try { pathname = decodeURIComponent(new URL(request.url, "http://127.0.0.1").pathname); }
    catch { response.writeHead(400).end("Bad request"); return; }
    const requested = resolve(root, `.${pathname === "/" ? "/index.html" : pathname}`);
    const escaped = requested !== root && !requested.startsWith(`${root}${sep}`);
    if (escaped) { response.writeHead(403).end("Forbidden"); return; }
    readFile(requested, (error, data) => {
      if (error) { response.writeHead(error.code === "ENOENT" ? 404 : 500).end("Not found"); return; }
      response.writeHead(200, {
        "Content-Type": mimeTypes.get(extname(requested).toLowerCase()) || "application/octet-stream",
        "Cache-Control": "no-store",
      });
      response.end(data);
    });
  });
}

function launchBrowser(executable, profile) {
  return new Promise((resolveLaunch, rejectLaunch) => {
    const args = [
      "--headless=new", "--disable-gpu", "--disable-dev-shm-usage", "--no-first-run", "--no-default-browser-check",
      "--disable-background-timer-throttling", "--autoplay-policy=no-user-gesture-required",
      `--user-data-dir=${profile}`, "--remote-debugging-port=0", "about:blank",
    ];
    if (process.platform === "linux") args.unshift("--no-sandbox");
    const child = spawn(executable, args, { windowsHide: true, stdio: ["ignore", "ignore", "pipe"] });
    let stderr = "";
    const timer = setTimeout(() => { child.kill(); rejectLaunch(new Error("browser debugging endpoint did not start")); }, 30000);
    child.stderr.setEncoding("utf8");
    child.stderr.on("data", (chunk) => {
      stderr += chunk;
      const match = stderr.match(/DevTools listening on (ws:\/\/[^\s]+)/);
      if (!match) return;
      clearTimeout(timer);
      const httpBase = match[1].replace(/^ws:/, "http:").replace(/\/devtools\/browser\/.*$/, "");
      resolveLaunch({ child, httpBase, stderr: () => stderr });
    });
    child.once("error", (error) => { clearTimeout(timer); rejectLaunch(error); });
    child.once("close", (code) => { clearTimeout(timer); rejectLaunch(new Error(`browser exited before debugging was ready (${code})\n${stderr.slice(-2000)}`)); });
  });
}

function connectCdp(url) {
  return new Promise((resolveSocket, rejectSocket) => {
    const socket = new WebSocket(url);
    const pending = new Map();
    const eventWaiters = new Map();
    let nextId = 1;
    socket.addEventListener("open", () => {
      resolveSocket({
        send(method, params = {}, timeoutMs = 30000) {
          return new Promise((resolveCommand, rejectCommand) => {
            const id = nextId++;
            const timer = setTimeout(() => { pending.delete(id); rejectCommand(new Error(`${method} timed out`)); }, timeoutMs);
            pending.set(id, { resolveCommand, rejectCommand, timer });
            socket.send(JSON.stringify({ id, method, params }));
          });
        },
        waitFor(method, timeoutMs = 30000) {
          return new Promise((resolveEvent, rejectEvent) => {
            const timer = setTimeout(() => { eventWaiters.delete(method); rejectEvent(new Error(`${method} timed out`)); }, timeoutMs);
            eventWaiters.set(method, { resolveEvent, timer });
          });
        },
        close() { socket.close(); },
      });
    }, { once: true });
    socket.addEventListener("error", () => rejectSocket(new Error(`could not connect to ${url}`)), { once: true });
    socket.addEventListener("message", (event) => {
      const message = JSON.parse(String(event.data));
      if (message.method && eventWaiters.has(message.method)) {
        const waiter = eventWaiters.get(message.method);
        eventWaiters.delete(message.method);
        clearTimeout(waiter.timer);
        waiter.resolveEvent(message.params);
      }
      if (!message.id || !pending.has(message.id)) return;
      const command = pending.get(message.id);
      pending.delete(message.id);
      clearTimeout(command.timer);
      if (message.error) command.rejectCommand(new Error(message.error.message));
      else command.resolveCommand(message.result);
    });
    socket.addEventListener("close", () => {
      for (const command of pending.values()) {
        clearTimeout(command.timer);
        command.rejectCommand(new Error("browser target closed"));
      }
      pending.clear();
      for (const waiter of eventWaiters.values()) clearTimeout(waiter.timer);
      eventWaiters.clear();
    });
  });
}

const baseHarnesses = [
  { name: "platform", path: "/tests/platform-browser-harness.html", timeout: 12000 },
  { name: "rack-presets", path: "/tests/rack-preset-browser-harness.html", timeout: 90000 },
  { name: "tape", path: "/tests/tape-browser-harness.html", timeout: 60000 },
  { name: "occlusion", path: "/tests/occlusion-browser-harness.html", timeout: 60000 },
  { name: "conference", path: "/tests/conference-browser-harness.html", timeout: 60000 },
  { name: "camcorder", path: "/tests/camcorder-browser-harness.html", timeout: 60000 },
  { name: "laptop-1366", path: "/tests/responsive-browser-harness.html?width=1366&height=768", timeout: 15000 },
  { name: "mobile-handoff", path: "/tests/responsive-browser-harness.html?width=390&height=844", timeout: 15000 },
  { name: "mobile-workflow", path: "/tests/mobile-workflow-browser-harness.html?width=390&height=844", timeout: 30000 },
];
const fullHarnesses = [
  { name: "parameter-influence-analog", path: "/tests/parameter-influence-harness.html?group=analog", timeout: 180000 },
  { name: "parameter-influence-digital", path: "/tests/parameter-influence-harness.html?group=digital", timeout: 300000 },
];

const executable = browserExecutable();
if (!executable) throw new Error("No supported Chromium browser found. Set LAE_BROWSER to a Chrome, Chromium, or Edge executable.");
const server = staticServer();
await new Promise((resolveListen, rejectListen) => {
  server.once("error", rejectListen);
  server.listen(0, "127.0.0.1", resolveListen);
});
const address = server.address();
const profile = mkdtempSync(join(tmpdir(), "lost-audio-browser-"));
const results = { browser: executable, platform: process.platform, full, harnesses: {} };
let browser;

try {
  browser = await launchBrowser(executable, profile);
  for (const harness of [...baseHarnesses, ...(full ? fullHarnesses : [])]) {
    const url = `http://127.0.0.1:${address.port}${harness.path}`;
    process.stdout.write(`Running ${harness.name}... `);
    const target = await fetch(`${browser.httpBase}/json/new?about%3Ablank`, { method: "PUT" }).then((response) => response.json());
    const cdp = await connectCdp(target.webSocketDebuggerUrl);
    await cdp.send("Page.enable");
    const loaded = cdp.waitFor("Page.loadEventFired", 30000);
    await cdp.send("Page.navigate", { url });
    await loaded;
    const completion = await cdp.send("Runtime.evaluate", {
      expression: `new Promise((resolve, reject) => { const started = Date.now(); const check = () => { if (document.body?.dataset.complete === "true") resolve(true); else if (Date.now() - started > ${harness.timeout}) reject(new Error("harness timeout")); else setTimeout(check, 50); }; check(); })`,
      awaitPromise: true,
      returnByValue: true,
    }, harness.timeout + 5000);
    if (completion.exceptionDetails) throw new Error(`${harness.name}: ${completion.exceptionDetails.text || "harness failed"}`);
    const dom = await cdp.send("Runtime.evaluate", { expression: "document.documentElement.outerHTML", returnByValue: true });
    const html = dom.result?.value || "";
    const match = html.match(/<pre id=["']results["'][^>]*>([\s\S]*?)<\/pre>/i);
    cdp.close();
    await fetch(`${browser.httpBase}/json/close/${target.id}`);
    if (!match) throw new Error(`${harness.name}: results block was not found`);
    const parsed = JSON.parse(decodeEntities(match[1]));
    const failures = collectSafetyFailures(parsed);
    if (harness.name === "platform" && parsed.supported !== true) failures.push("platform.supported=false");
    if (harness.name === "laptop-1366" && (parsed.horizontalOverflow || !parsed.workstationVisible || parsed.mobileVisible)) failures.push("desktop responsive contract failed");
    if (harness.name === "mobile-handoff" && (
      parsed.horizontalOverflow
      || !parsed.workstationVisible
      || parsed.mobileVisible
      || !parsed.mobileDockVisible
      || parsed.visiblePanels?.length !== 1
    )) failures.push("mobile workstation contract failed");
    if (harness.name === "mobile-workflow" && parsed.pass !== true) failures.push("mobile workflow contract failed");
    if (failures.length) throw new Error(`${harness.name}: ${failures.join(", ")}`);
    results.harnesses[harness.name] = parsed;
    process.stdout.write("passed\n");
  }
} finally {
  if (browser) {
    await new Promise((resolveExit) => {
      if (browser.child.exitCode !== null) { resolveExit(); return; }
      const timer = setTimeout(resolveExit, 5000);
      browser.child.once("close", () => { clearTimeout(timer); resolveExit(); });
      browser.child.kill();
    });
  }
  await new Promise((resolveClose) => server.close(resolveClose));
  rmSync(profile, { recursive: true, force: true, maxRetries: 10, retryDelay: 200 });
  mkdirSync(dirname(outputPath), { recursive: true });
  writeFileSync(outputPath, `${JSON.stringify(results, null, 2)}\n`);
}

console.log(`Browser validation passed: ${Object.keys(results.harnesses).length} harnesses (${relative(root, outputPath)}).`);
