# Native VST3 status and build guide

## Current status

The repository contains ten JUCE projects. They build VST3 and Standalone targets, preserve existing plugin identities, and contain useful MVP DSP/UI work. They are not currently the sonic authority and must not be described as equivalent to the current web workstation.

Native V2 work begins after the Linux and broader platform/device validation phase.

## Plugin identities

These four-character plugin codes and bundle IDs are compatibility-sensitive. Changing one can make a DAW treat an update as a different plugin; reusing one can make two plugins replace each other.

| Project | Product | Code | Bundle ID |
| --- | --- | --- | --- |
| `transmission-vst3` | Transmission Engine | `TrnE` | `com.lostaudio.transmissionengine` |
| `tape-vst3` | Tape Engine | `TpEg` | `com.lostaudio.tapeengine` |
| `television-vst3` | Television Engine | `TvEg` | `com.lostaudio.televisionengine` |
| `cartridge-vst3` | Cartridge Engine | `CrEg` | `com.lostaudio.cartridgeengine` |
| `cd-vst3` | CD Engine | `CdEg` | `com.lostaudio.cdengine` |
| `comms-vst3` | Comms Engine | `CmEg` | `com.lostaudio.commsengine` |
| `conference-vst3` | Conference Engine | `CfEg` | `com.lostaudio.conferenceengine` |
| `camcorder-vst3` | Camcorder Engine | `CcEg` | `com.lostaudio.camcorderengine` |
| `occlusion-vst3` | Occlusion Engine | `OcEg` | `com.lostaudio.occlusionengine` |
| `openmicnight-vst3` | Open Mic Night | `OmNt` | `com.lostaudio.openmicnight` |

Camcorder’s `CcEg` identity is evidence-backed; see the [published release audit](release-audit.md).

## Prerequisites

- CMake 3.22+
- a C++20 compiler
- a JUCE source checkout
- platform plugin-development prerequisites required by JUCE

JUCE is external and not pinned yet. Record its exact commit in every diagnostic build report.

## Configure all plugins

Windows / Visual Studio:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR="C:/path/to/JUCE"
cmake --build build --config Release --parallel
```

Linux / Ninja:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR=/path/to/JUCE
cmake --build build --parallel
```

The root `CMakeLists.txt` configures all ten projects. An individual `*-vst3/` directory can also be configured alone with the same `JUCE_DIR` argument.

`COPY_PLUGIN_AFTER_BUILD` is disabled. Builds must not silently overwrite installed release plugins.

## Before distributing a native build

Require:

- clean configure and rebuild from an empty build directory;
- unique component/controller identities;
- plugin validation tooling appropriate to the platform;
- mono and stereo processing;
- 44.1, 48, and 96 kHz coverage;
- small and large buffer sizes;
- parameter automation and smoothing;
- state save/reopen and old-state migration;
- preset loading and naming;
- bypass, silence, denormal, NaN/Inf, feedback, and output-ceiling checks;
- representative DAW scans and sessions;
- level-matched comparisons against frozen web-reference renders;
- a listening review covering subtle, authentic, damaged, and exaggerated ranges;
- explicit signing/notarization decisions for each release platform.

## V2 architecture target

The current projects duplicate DSP, parameters, preset definitions, and UI behavior. V2 should introduce:

1. a realtime-safe portable DSP core without JUCE UI dependencies;
2. one versioned parameter/preset schema with migration;
3. adapters for JUCE and WebAssembly/AudioWorklet;
4. a shared Surface/Advanced interaction system with distinct device art direction;
5. deterministic seeds and reference-render capture;
6. a common QA matrix;
7. reuse by individual plugins, Lost Audio Suite slots, and Lost Audio Sequencer lanes.

Transmission should be the first vertical slice only after platform validation. Do not multiply a new architecture across ten plugins until its sound, automation, recall, UI, and host behavior are approved.
