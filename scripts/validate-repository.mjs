import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, extname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const failures = [];
const counts = { javascript: 0, json: 0, markdown: 0, references: 0, plugins: 0, presets: 0 };
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

function extractInitializer(source, arrayName) {
  const declaration = new RegExp(`\\b${arrayName.replace(/[.*+?^${}()|[\\]\\\\]/g, "\\$&")}\\b\\s*(?:\\[\\s*\\])?\\s*(?:=\\s*)?(?=\\{)`).exec(source);
  if (!declaration) return null;
  const open = source.indexOf("{", declaration.index + declaration[0].length);
  if (open < 0) return null;
  let depth = 0;
  let quote = null;
  let escaped = false;
  for (let index = open; index < source.length; index++) {
    const character = source[index];
    if (quote) {
      if (escaped) escaped = false;
      else if (character === "\\") escaped = true;
      else if (character === quote) quote = null;
      continue;
    }
    if (character === '"' || character === "'") { quote = character; continue; }
    if (character === "{") depth++;
    else if (character === "}" && --depth === 0) return source.slice(open, index + 1);
  }
  return null;
}

function parsePresetBank(source, arrayName) {
  const initializer = extractInitializer(source, arrayName);
  if (!initializer) return null;
  const starts = [...initializer.matchAll(/\{\s*"([^"]+)"\s*,\s*\{/g)];
  return starts.map((match, index) => {
    const recipe = initializer.slice(match.index, starts[index + 1]?.index ?? initializer.length);
    const settings = [...recipe.matchAll(/\{\s*"([A-Za-z][A-Za-z0-9]*)"\s*,\s*(-?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?f?)\s*\}/g)]
      .map((setting) => ({ id: setting[1], value: Number.parseFloat(setting[2]) }));
    return { name: match[1], settings };
  });
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
  "native/core/include/lost_audio/core/CDProcessor.h",
  "native/core/src/CDProcessor.cpp", "native/core/tests/CDProcessorTests.cpp",
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
const expectedPluginIdentities = new Map([
  ["brain-cruncher-vst3", { product: "Brain Cruncher", code: "BrCr", bundle: "com.bedigital.braincruncher" }],
  ["transmission-vst3", { product: "Transmission Engine", code: "TrnE", bundle: "com.lostaudio.transmissionengine" }],
  ["tape-vst3", { product: "Tape Engine", code: "TpEg", bundle: "com.lostaudio.tapeengine" }],
  ["television-vst3", { product: "Television Engine", code: "TvEg", bundle: "com.lostaudio.televisionengine" }],
  ["cartridge-vst3", { product: "Cartridge Engine", code: "CrEg", bundle: "com.lostaudio.cartridgeengine" }],
  ["cd-vst3", { product: "CD Engine", code: "CdEg", bundle: "com.lostaudio.cdengine" }],
  ["comms-vst3", { product: "Comms Engine", code: "CmEg", bundle: "com.lostaudio.commsengine" }],
  ["conference-vst3", { product: "Conference Engine", code: "CfEg", bundle: "com.lostaudio.conferenceengine" }],
  ["camcorder-vst3", { product: "Camcorder Engine", code: "CcEg", bundle: "com.lostaudio.camcorderengine" }],
  ["occlusion-vst3", { product: "Occlusion Engine", code: "OcEg", bundle: "com.lostaudio.occlusionengine" }],
  ["openmicnight-vst3", { product: "Open Mic Night", code: "OmNt", bundle: "com.lostaudio.openmicnight" }],
  ["suite-vst3", { product: "Lost Audio Suite", code: "LaSu", bundle: "com.bedigital.lostaudiosuite" }],
  ["sequencer-vst3", { product: "Lost Audio Sequencer", code: "LaSq", bundle: "com.bedigital.lostaudiosequencer" }],
]);
const presetArrayNames = new Map([
  ["brain-cruncher-vst3", "factoryPresets"],
  ["transmission-vst3", "presets"],
  ["tape-vst3", "kPresets"],
  ["television-vst3", "presets"],
  ["cartridge-vst3", "presets"],
  ["cd-vst3", "presets"],
  ["comms-vst3", "presets"],
  ["conference-vst3", "presets"],
  ["camcorder-vst3", "presets"],
  ["occlusion-vst3", "presetsData"],
  ["openmicnight-vst3", "presets"],
]);
for (const entry of readdirSync(root, { withFileTypes: true }).filter((item) => item.isDirectory() && item.name.endsWith("-vst3"))) {
  const cmakePath = join(root, entry.name, "CMakeLists.txt");
  const processorPath = join(root, entry.name, "src", "PluginProcessor.cpp");
  const editorPath = join(root, entry.name, "src", "PluginEditor.cpp");
  if (!existsSync(cmakePath) || !existsSync(processorPath) || !existsSync(editorPath)) {
    fail(`${entry.name}: incomplete JUCE project`);
    continue;
  }
  const cmake = readFileSync(cmakePath, "utf8");
  const processor = readFileSync(processorPath, "utf8");
  const editor = readFileSync(editorPath, "utf8");
  const code = cmake.match(/PLUGIN_CODE\s+(\w+)/)?.[1];
  const bundle = cmake.match(/BUNDLE_ID\s+"([^"]+)"/)?.[1];
  const company = cmake.match(/COMPANY_NAME\s+"([^"]+)"/)?.[1];
  const manufacturer = cmake.match(/PLUGIN_MANUFACTURER_CODE\s+(\w+)/)?.[1];
  const product = cmake.match(/PRODUCT_NAME\s+"([^"]+)"/)?.[1];
  if (!code || code.length !== 4) fail(`${entry.name}: missing four-character PLUGIN_CODE`);
  else if (pluginCodes.has(code)) fail(`${entry.name}: PLUGIN_CODE ${code} duplicates ${pluginCodes.get(code)}`);
  else pluginCodes.set(code, entry.name);
  if (!bundle) fail(`${entry.name}: missing BUNDLE_ID`);
  else if (bundleIds.has(bundle)) fail(`${entry.name}: BUNDLE_ID ${bundle} duplicates ${bundleIds.get(bundle)}`);
  else bundleIds.set(bundle, entry.name);
  if (company !== "B&E Digital") fail(`${entry.name}: visible publisher must be B&E Digital`);
  if (manufacturer !== "LsAu") fail(`${entry.name}: compatibility-sensitive manufacturer code changed`);
  const expected = expectedPluginIdentities.get(entry.name);
  if (!expected) fail(`${entry.name}: plugin is missing from the identity contract`);
  else {
    if (code !== expected.code) fail(`${entry.name}: PLUGIN_CODE changed from ${expected.code} to ${code}`);
    if (bundle !== expected.bundle) fail(`${entry.name}: BUNDLE_ID changed from ${expected.bundle} to ${bundle}`);
    if (product !== expected.product) fail(`${entry.name}: PRODUCT_NAME changed from ${expected.product} to ${product}`);
  }
  if (entry.name === "suite-vst3") {
    if (!editor.includes('"LOST AUDIO SUITE"')) fail("suite-vst3: visible Lost Audio Suite title is missing");
  } else if (entry.name === "sequencer-vst3") {
    if (!editor.includes('"EFFECT SEQUENCER"')) fail("sequencer-vst3: visible sequencer title is missing");
  } else if (/LOST AUDIO/i.test(editor)) {
    fail(`${entry.name}: individual plugin UI still carries Lost Audio branding`);
  }
  if (editor.includes("setEnabled(false)")) fail(`${entry.name}: editor contains a disabled control path`);
  if (entry.name !== "suite-vst3") {
    const parameterIds = new Set([...processor.matchAll(/AudioParameter(?:Float|Bool|Choice|Int)>\(\s*"([^"]+)"/g)].map((match) => match[1]));
    for (const id of parameterIds) {
      const escaped = id.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
      const occurrences = [...processor.matchAll(new RegExp(`"${escaped}"`, "g"))].length;
      if (occurrences < 2) fail(`${entry.name}: parameter ${id} is declared but never read by the audio adapter`);
    }
    const editorIds = new Set();
    for (const match of editor.matchAll(/(?:addKnob|addChoice|addSwitch|knob|choice)\(\s*(?:"([^"]+)"|[^,;\n]+,\s*"([^"]+)")/g)) editorIds.add(match[1] ?? match[2]);
    for (const match of editor.matchAll(/Attachment>\([^,]+,\s*"([^"]+)"/g)) editorIds.add(match[1]);
    for (const match of editor.matchAll(/(?:getParameter|getRawParameterValue|setParameter|get|set)\(\s*"([^"]+)"/g)) editorIds.add(match[1]);
    for (const id of editorIds) if (!parameterIds.has(id)) fail(`${entry.name}: editor references missing parameter ${id}`);

    const presetArrayName = presetArrayNames.get(entry.name);
    if (presetArrayName) {
      const presets = parsePresetBank(editor, presetArrayName);
      if (!presets?.length) fail(`${entry.name}: could not parse factory preset bank ${presetArrayName}`);
      else {
        const names = new Set();
        const recipes = new Map();
        let performanceCount = 0;
        for (const preset of presets) {
          counts.presets++;
          if (names.has(preset.name)) fail(`${entry.name}: duplicate preset name ${preset.name}`);
          names.add(preset.name);
          if (!preset.settings.length) fail(`${entry.name}: preset ${preset.name} has no parameter recipe`);
          for (const setting of preset.settings) {
            if (!parameterIds.has(setting.id)) fail(`${entry.name}: preset ${preset.name} references missing parameter ${setting.id}`);
          }
          const signature = preset.settings.map(({ id, value }) => `${id}=${value}`).sort().join("|");
          if (recipes.has(signature)) fail(`${entry.name}: preset ${preset.name} duplicates the recipe for ${recipes.get(signature)}`);
          else recipes.set(signature, preset.name);

          if (preset.name.startsWith("PLAY - ")) {
            performanceCount++;
            const values = new Map(preset.settings.map(({ id, value }) => [id, value]));
            const blendId = entry.name === "cartridge-vst3" ? "wet" : "mix";
            const outputId = entry.name === "brain-cruncher-vst3" ? "outputGain" : "outGain";
            for (const requiredId of [blendId, outputId, "ceiling"])
              if (!values.has(requiredId)) fail(`${entry.name}: performance preset ${preset.name} must explicitly set ${requiredId}`);
            for (const [id, value] of values) {
              if (/(?:noise|hiss|hum|static|windLevel|bedLevel|sfxLevel|camBedLevel|crowdLevel)$/i.test(id) && value > .25)
                fail(`${entry.name}: performance preset ${preset.name} sets ${id} above the musical noise-floor limit`);
              if (/Probability$/i.test(id) && value > .40)
                fail(`${entry.name}: performance preset ${preset.name} sets ${id} above the bounded trigger-density limit`);
            }
          }
        }
        if (performanceCount < 4) fail(`${entry.name}: expected at least four PLAY performance presets, found ${performanceCount}`);
      }
    }
  }
  counts.plugins++;
}
if (counts.plugins !== expectedPluginIdentities.size) fail(`expected ${expectedPluginIdentities.size} VST3 projects, found ${counts.plugins}`);

const accessSource = readFileSync(join(root, "lame/src/main.js"), "utf8");
if (!accessSource.includes('const ACCESS_GATE_ENABLED = /(^|\\.)bande\\.digital$/i.test(window.location.hostname);')) {
  fail("source-host access boundary is missing; local/independent builds must not depend on the B&E signup API");
}

if (failures.length) {
  console.error(`Repository validation failed with ${failures.length} problem(s):`);
  for (const problem of failures) console.error(`- ${problem}`);
  process.exit(1);
}

console.log(`Repository validation passed: ${counts.javascript} JavaScript files, ${counts.json} JSON files, ${counts.markdown} Markdown files, ${counts.references} local references, ${counts.plugins} plugin identities, ${counts.presets} individual factory presets.`);
