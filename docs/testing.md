# Testing and listening QA

Lost Audio Engine uses layered evidence. Passing source checks does not prove sound quality, and a good listening impression does not prove every parameter is wired.

## Fast repository validation

Requirements: Node.js 20+ and Python 3.10+.

```powershell
node scripts/validate-repository.mjs
node tests/platform-capabilities.test.mjs
node tests/obfuscation-body-harness.mjs
python tests/parity/parity.py generate --out tests/parity/fixtures
```

The repository validator checks required paths, JavaScript module syntax, JSON manifests, local module/worklet references, Markdown links, unique plugin identities, and the absence of committed root release ZIPs. The platform unit test exercises supported, insecure, prefixed, and incomplete browser capability surfaces without launching a browser.

The Node obfuscation harness verifies deterministic rattle/body behavior, finite output, an exact disabled branch, and bounded safety behavior without requiring a browser.

The parity generator creates deterministic PCM16 fixtures at 44.1, 48, and 96 kHz. Generated WAVs are ignored; `tests/parity/fixtures/manifest.json` records the fixture contract.

## Browser DSP harnesses

The repeatable automated route is:

```powershell
$env:LAE_BROWSER = "C:\Program Files\Google\Chrome\Application\chrome.exe"
node scripts/run-browser-harnesses.mjs --full
```

Omit `LAE_BROWSER` when Chrome, Chromium, or Edge is in a standard installation location. The runner uses an isolated headless profile, a loopback-only server, and the browser debugging protocol; it does not reuse or focus the user's normal browser. Results are written to `.artifacts/platform-browser-results.json`. The full run fails on missing presets, non-finite samples, ceiling violations, dead/too-subtle parameter comparisons, laptop overflow, or an incorrect mobile handoff.

For manual investigation, serve the repository first:

```powershell
python -m http.server 5173
```

Then open the relevant route:

| Harness | Route | Purpose |
| --- | --- | --- |
| Parameter influence | `/tests/parameter-influence-harness.html?group=analog` | Tape, Television, and Obfuscation controls alter output |
| Parameter influence | `/tests/parameter-influence-harness.html?group=digital` | Transmission, Comms, Conference, Cartridge, CD, and Camcorder controls alter output |
| Rack presets | `/tests/rack-preset-browser-harness.html` | Protected rack presets build valid chains and remain output-safe |
| Tape | `/tests/tape-browser-harness.html` | Latency, partial wet behavior, and representative preset safety |
| Obfuscation | `/tests/occlusion-browser-harness.html` | Material, construction, macro, and resonant-body differentiation |
| Conference | `/tests/conference-browser-harness.html` | Codec/network preset safety and failure behavior |
| Camcorder | `/tests/camcorder-browser-harness.html` | Format/mic preset safety and wind/corruption isolation |
| Responsive | `/tests/responsive-browser-harness.html?width=1366&height=768` | Laptop composition and horizontal-overflow check |
| Responsive | `/tests/responsive-browser-harness.html?width=390&height=844` | Deliberate mobile handoff |
| Platform | `/tests/platform-browser-harness.html` | Startup API/capability surface in the actual browser |

A browser harness is complete when `document.body.dataset.complete` becomes `"true"`. Save the JSON output with the commit, browser version, operating system, and sample rate when using it as release evidence.

## Parameter-wiring gate

For every exposed parameter:

1. render controlled low and high states from the same deterministic input and seed;
2. confirm a meaningful output difference or document why the parameter is state/event dependent;
3. test its module surface and inspector binding;
4. save/reload it in a user preset;
5. automate it if exposed to automation;
6. compare realtime playback with offline export.

A moving label, knob, or fader does not count as DSP evidence.

## Listening matrix

Use legally shareable or self-created fixtures:

- spoken voice and dialogue;
- sung voice;
- full music;
- drums and sharp transients;
- ambience and low-level room tone;
- silence for self-noise and event behavior;
- stereo material for image and collapse checks;
- quiet and hot input levels.

Listen at 44.1, 48, and 96 kHz where supported. Compare dry, subtle, authentic, damaged, and exaggerated states at matched perceived loudness.

Score each engine for:

- identity: does it evoke the intended device/path rather than generic filtering?
- realism: are failure mechanics plausible at subtle settings?
- expressive range: can it cross into useful exaggeration without immediately collapsing?
- intelligibility: can dialogue remain usable when the preset claims it should?
- motion: do random or cyclic artifacts feel causal rather than arbitrary?
- noise behavior: are beds level-controlled and independent where expected?
- safety: no non-finite samples, runaway feedback, pathological discontinuities, or default clipping;
- recall: preset/session reopen produces the same intended state.

## Native parity

`tests/parity/parity.py compare` reports maximum absolute error, RMSE, and normalized correlation for matching WAVs:

```powershell
python tests/parity/parity.py compare web-reference.wav native-port.wav
```

This is diagnostic evidence, not a universal pass threshold. Nonlinear and stochastic models require the same parameter state, seed, sample rate, channel layout, and master-chain decision. Final approval still requires level-matched listening.

## Release evidence

Record:

- commit SHA and dirty/clean status;
- browser/DAW, operating system, CPU architecture, sample rate, and buffer size;
- fixture and output hashes;
- preset and full parameter state;
- deterministic seed where applicable;
- automated results;
- listening reviewer and result;
- known exceptions or untested targets.
