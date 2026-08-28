# Native VST3 status and build guide

## Current status

The repository contains ten JUCE projects. They build VST3 and Standalone targets, preserve existing plugin identities, and contain useful MVP DSP/UI work. They are not currently the sonic authority and must not be described as equivalent to the current web workstation.

Native V2 work is active. Broader Linux/browser/device validation remains documented backlog rather than a blocker. Tape is the proving engine for the portable core because it exercises deterministic modulation, stochastic damage, noise, nonlinear color, latency, safety, presets, and embedded mechanical media.

## Native V2 foundation

`native/core` is a C++20 library with no JUCE dependency. Its first processor ports the approved browser Tape worklet equations and deterministic xorshift state. It owns transport delay, wow/flutter/drift, downward compression, gain-compensated saturation, dropout scheduling, hiss/hum, output gain, limiting, and reported latency. The Tape JUCE adapter retains host parameters, state, embedded transport SFX, and post-core cabinet filters.

The Tape adapter now:

- reports the web graph's 12 ms transport latency to the host;
- uses the shared core instead of its divergent duplicated processor loop;
- uses shared macro mapping for UI and factory presets;
- writes `engineId=tape` and `schemaVersion=2` while accepting unversioned V1 APVTS states;
- preserves the existing `TpEg` plugin identity and parameter identifiers.

Its former fixed 860 x 540 stock-tab editor has been retired. Tape V2 now uses a resizable, device-specific cassette surface with animated reels, live output metering, four readable character macros, and a deliberate Surface/Advanced split. Advanced keeps the entire signal path visible in four named sections without scrolling. The minimum 820 x 560 layout is intended for laptop hosts; the editor scales to 1600 x 1000.

The dependency-free native test checks reported/actual latency, seed determinism, host-block-size invariance, finite output, ceiling safety, and macro influence. CI additionally compiles the Tape VST3 target and its editor against the pinned JUCE revision, then produces a downloadable Windows VST3 test bundle after the source, DSP, native, and browser gates pass. Audible parity, smoothing under automation, complete preset parity, DAW scanning, and hands-on plugin UI approval remain open gates.

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
- network access for the pinned JUCE checkout, or an existing JUCE source checkout
- platform plugin-development prerequisites required by JUCE

The root build pins JUCE 9.0.1 at commit `e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8`. Pass `JUCE_DIR` to use an audited local checkout instead of the pinned fetch.

To build only the portable core and its tests:

```powershell
cmake -S . -B build-core -DLAE_BUILD_PLUGINS=OFF -DBUILD_TESTING=ON
cmake --build build-core --config Release --parallel
ctest --test-dir build-core -C Release --output-on-failure
```

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

Tape is the first vertical slice. Do not multiply the architecture across ten plugins until its sound, automation, recall, UI, and host behavior are approved. Transmission follows after the Tape slice proves the reusable boundary.
