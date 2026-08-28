# Lost Audio Engine

[![Repository validation](https://github.com/SageAzakaela/LOST-AUDIO-ENGINE/actions/workflows/validate.yml/badge.svg)](https://github.com/SageAzakaela/LOST-AUDIO-ENGINE/actions/workflows/validate.yml)

Lost Audio Engine is a browser-based modular audio degradation workstation and a family of character effects for making clean recordings sound old, strange, distant, obstructed, damaged, or unreliable.

This is not a single “lo-fi” filter. It is a playable signal path built from device, medium, transmission, codec, and material models. The useful range runs from believable and subtle through damaged and deliberately exaggerated.

- [Use the B&E Digital edition](https://bande.digital/tools/lost/)
- [Use the itch.io edition](https://azakaela.itch.io/lost-audio-engine)
- [Report a bug or request](https://github.com/SageAzakaela/LOST-AUDIO-ENGINE/issues)

## What is here

The web workstation is the current product and sonic reference. It provides:

- a drag-and-drop, left-to-right device rack;
- realtime auditioning and deterministic offline WAV export through the same graph;
- source trim, monitor volume, dry/processed comparison, waveform feedback, and transport controls;
- focused device surfaces with deeper inspectors;
- factory device presets and twenty-two protected rack presets;
- file-time automation for exposed rack and mastering controls;
- a final chain with ten-band EQ, adaptive noise reduction, saturation, delay, reverb, compression, limiting, metering, and output protection;
- local browser processing: loaded audio is not uploaded by the workstation.

Nine device engines are available in the rack:

| Engine | Models |
| --- | --- |
| Transmission | portable radio, tuning behavior, unstable reception, noisy transmission |
| Comms | telephone, cellphone, intercom, PA, alarm, and small communications hardware |
| Conference | Discord/Zoom/Skype/cellular-style codecs, packet loss, jitter, PLC, AGC, and suppression |
| Tape | cassette and VHS transport wear, saturation, compression, hiss, wow, flutter, and dropout |
| Television | CRT speaker and broadcast coloration, hum, static, and cabinet behavior |
| Cartridge | old cartridge/console playback, constrained converters, speaker character, and controlled digital grit |
| CD | optical tracking faults, scratches, buffered skips, concealment, jitter, and damage patterns |
| Camcorder | consumer recording formats, camera microphones, AGC, handling, wind, motor bleed, and corruption |
| Obfuscation | transmission through walls/materials, cavities, panel modes, leakage, rattles, and rooms |

Open Mic Night currently remains a standalone experiment and native-plugin source rather than a rack module.

## Run the web workstation locally

AudioWorklet modules require HTTP(S); opening `index.html` directly from disk is not supported.

```powershell
python -m http.server 5173
```

Open `http://localhost:5173/` in a desktop browser. The full workstation intentionally requires a viewport at least 1024 pixels wide; phones receive a desktop-only handoff.

Local and independently hosted source builds are unlimited. The optional fifteen-minute/email unlock is enabled only on `bande.digital`, where it connects to B&E’s own signup API. Audio processing remains local in either mode.

## Repository map

```text
.
├── index.html                   Current web workstation shell
├── lame/                        Rack UI/styles, orchestration, mastering, automation
├── src/, transmission/          Transmission graph and legacy standalone page
├── styles.css                   Legacy standalone Transmission styling
├── *-engine/                    Standalone web engine implementations
├── *-vst3/                      Existing JUCE VST3 + Standalone projects
├── audio/                       Shared Transmission source assets and manifests
├── tests/                       DSP, parameter-influence, responsive, and parity harnesses
├── scripts/                     Repository validation utilities
├── docs/                        Architecture, QA, plans, and historical evidence
├── .github/                     Issue forms and continuous validation
└── make-itch-zip.py             Reproducible web release packager
```

The directory name `lame/` is historical shorthand for “Lost Audio Media Engine.” It is retained because deployed URLs and module references depend on it; the public product name is always **Lost Audio Engine**.

## Documentation

- [Documentation index](docs/README.md)
- [Architecture and signal flow](docs/architecture.md)
- [Web workstation guide](docs/web-workstation.md)
- [Testing and listening QA](docs/testing.md)
- [Platform and device integration](docs/platform-integration.md)
- [Native VST3 status and build guide](docs/vst3.md)
- [Product roadmap](docs/roadmap.md)
- [Design constraints](docs/design-constraints.md)
- [Expansion priorities](docs/expansion-ideas.md)
- [Feedback tracker](docs/feedback-tracker.md)
- [Published release audit](docs/release-audit.md)
- [Contributing](CONTRIBUTING.md)

## Validate the source

The baseline validation has no package installation step:

```powershell
node scripts/validate-repository.mjs
node tests/obfuscation-body-harness.mjs
python tests/parity/parity.py generate --out tests/parity/fixtures
```

Browser DSP harnesses and the manual listening matrix are documented in [docs/testing.md](docs/testing.md). Numerical tests protect wiring, determinism, safety, and parameter influence; they do not replace level-matched listening.

## Build the existing native plugins

The repository contains ten JUCE projects. They are historical MVP ports and are **not yet at parity with the web reference**.

```powershell
cmake -S . -B build -DJUCE_DIR=C:\path\to\JUCE
cmake --build build --config Release
```

JUCE is external and not yet pinned. Record the exact JUCE commit for diagnostic builds. See [docs/vst3.md](docs/vst3.md) before treating any native build as a release.

## Current direction

The release order is deliberate:

1. preserve, organize, document, and validate the web reference;
2. complete Linux and broader desktop/device integration work;
3. rebuild the individual VST3s against the approved reference;
4. create the all-in-one Lost Audio Suite;
5. create the host-synced Lost Audio Sequencer.

Ductwork and Water Boundary are the first approved space-engine expansions. Dictaphone, Surveillance, Lossy Media, and Turntable are preferred later device additions.

## Releases and generated files

Generated ZIPs and plugin bundles are intentionally excluded from the source tree. Run:

```powershell
python make-itch-zip.py
```

The package is written to `dist/LostAudioEngine-web-itchio.zip`. Publish binaries through itch.io or GitHub Releases and record their hashes in the release notes; do not commit them beside source.

## Source-use notice

No source license is currently published. Public visibility and downloadable builds do not grant permission to copy, redistribute, or create derivative source releases. A license and bundled-audio provenance decision is a required release task before broader code contributions or reuse are invited.
