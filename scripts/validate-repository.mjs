import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, extname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const failures = [];
const counts = { javascript: 0, json: 0, markdown: 0, references: 0, plugins: 0 };
const ignoredDirectories = new Set([".git", ".artifacts", "build", "build-core", "build-validation", "dist", "node_modules", "CMakeFiles"]);

function fail(message) {
  failures.push(message);
}

function walk(directory, files = []) {
  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    if (entry.isDirectory() && (ignoredDirectories.has(entry.name) || entry.name.startsWith("build-") || entry.name.startsWith("cmake-build-") || entry.name.endsWith("_artefacts"))) continue;
    const absolute = join(directory, entry.name);
    if (entry.isDirectory()) walk(absolute, files);
    else if (entry.isFile()) files.push(absolute);
  }
  return files;
}

function cleanReference(value) {
  return value.split(/[?#]/, 1)[0];
}

function checkRelativeReference(sourceFile, reference, label) {
  if (!reference.startsWith(".")) return;
  const target = resolve(dirname(sourceFile), cleanReference(reference));
  counts.references++;
  if (!existsSync(target)) fail(`${relative(root, sourceFile)}: missing ${label} ${reference}`);
}

const required = [
  "README.md", "CONTRIBUTING.md", "CMakeLists.txt", "index.html", "lame/src/main.js", "lame/src/audio/graph.js",
  "docs/architecture.md", "docs/testing.md", "docs/platform-integration.md", "docs/vst3.md", "docs/roadmap.md",
  "lame/src/platform.js", "scripts/run-browser-harnesses.mjs", "tests/platform-capabilities.test.mjs",
  "tests/platform-browser-harness.html", "tests/platform-browser-harness.js",
  "native/core/CMakeLists.txt", "native/core/include/lost_audio/core/TapeProcessor.h",
  "native/core/src/TapeProcessor.cpp", "native/core/tests/TapeProcessorTests.cpp",
  "native/core/include/lost_audio/core/TransmissionProcessor.h",
  "native/core/src/TransmissionProcessor.cpp", "native/core/tests/TransmissionProcessorTests.cpp",
  "native/core/include/lost_audio/core/CommsProcessor.h",
  "native/core/src/CommsProcessor.cpp", "native/core/tests/CommsProcessorTests.cpp",
  "installer/windows/TapeEngine.iss", "scripts/build-tape-windows-installer.ps1",
  "tests/obfuscation-body-harness.mjs", "tests/parity/parity.py", ".github/workflows/validate.yml",
];
for (const path of required) if (!existsSync(join(root, path))) fail(`required path is missing: ${path}`);

for (const entry of readdirSync(root)) {
  if (extname(entry).toLowerCase() === ".zip" && statSync(join(root, entry)).isFile()) fail(`generated release ZIP must not live at repository root: ${entry}`);
}

const files = walk(root);
for (const file of files) {
  const extension = extname(file).toLowerCase();
  const source = [".js", ".mjs", ".json", ".md"].includes(extension) ? readFileSync(file, "utf8") : "";

  if (extension === ".js" || extension === ".mjs") {
    counts.javascript++;
    const syntax = spawnSync(process.execPath, ["--input-type=module", "--check"], { input: source, encoding: "utf8" });
    if (syntax.status !== 0) fail(`${relative(root, file)}: JavaScript syntax failed\n${syntax.stderr.trim()}`);
    for (const match of source.matchAll(/(?:from\s+|import\s*\()\s*["']([^"']+)["']/g)) checkRelativeReference(file, match[1], "module");
    for (const match of source.matchAll(/new URL\(\s*["']([^"']+)["']\s*,\s*import\.meta\.url\s*\)/g)) checkRelativeReference(file, match[1], "URL asset");
  }

  if (extension === ".json") {
    counts.json++;
    try { JSON.parse(source); } catch (error) { fail(`${relative(root, file)}: invalid JSON (${error.message})`); }
  }

  if (extension === ".md") {
    counts.markdown++;
    for (const match of source.matchAll(/\[[^\]]*\]\(([^)]+)\)/g)) {
      const reference = match[1].trim().replace(/^<|>$/g, "");
      if (/^(?:https?:|mailto:|#)/i.test(reference)) continue;
      checkRelativeReference(file, reference, "Markdown link");
    }
  }
}

const pluginCodes = new Map();
const bundleIds = new Map();
for (const entry of readdirSync(root, { withFileTypes: true }).filter((item) => item.isDirectory() && item.name.endsWith("-vst3"))) {
  const cmakePath = join(root, entry.name, "CMakeLists.txt");
  const processorPath = join(root, entry.name, "src", "PluginProcessor.cpp");
  const editorPath = join(root, entry.name, "src", "PluginEditor.cpp");
  if (!existsSync(cmakePath) || !existsSync(processorPath) || !existsSync(editorPath)) {
    fail(`${entry.name}: incomplete JUCE project`);
    continue;
  }
  const cmake = readFileSync(cmakePath, "utf8");
  const code = cmake.match(/PLUGIN_CODE\s+(\w+)/)?.[1];
  const bundle = cmake.match(/BUNDLE_ID\s+"([^"]+)"/)?.[1];
  if (!code || code.length !== 4) fail(`${entry.name}: missing four-character PLUGIN_CODE`);
  else if (pluginCodes.has(code)) fail(`${entry.name}: PLUGIN_CODE ${code} duplicates ${pluginCodes.get(code)}`);
  else pluginCodes.set(code, entry.name);
  if (!bundle) fail(`${entry.name}: missing BUNDLE_ID`);
  else if (bundleIds.has(bundle)) fail(`${entry.name}: BUNDLE_ID ${bundle} duplicates ${bundleIds.get(bundle)}`);
  else bundleIds.set(bundle, entry.name);
  counts.plugins++;
}

const accessSource = readFileSync(join(root, "lame/src/main.js"), "utf8");
if (!accessSource.includes('const ACCESS_GATE_ENABLED = /(^|\\.)bande\\.digital$/i.test(window.location.hostname);')) {
  fail("source-host access boundary is missing; local/independent builds must not depend on the B&E signup API");
}

if (failures.length) {
  console.error(`Repository validation failed with ${failures.length} problem(s):`);
  for (const problem of failures) console.error(`- ${problem}`);
  process.exit(1);
}

console.log(`Repository validation passed: ${counts.javascript} JavaScript files, ${counts.json} JSON files, ${counts.markdown} Markdown files, ${counts.references} local references, ${counts.plugins} plugin identities.`);
