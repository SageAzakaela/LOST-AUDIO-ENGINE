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

The dependency-free native test checks reported/actual latency, seed determinism, host-block-size invariance, finite output, ceiling safety, and macro influence. CI additionally compiles the Tape VST3 target and its editor against the pinned JUCE revision, then preserves an explicitly labelled unsigned developer bundle after the source, DSP, native, and browser gates pass. Audible parity, smoothing under automation, complete preset parity, DAW scanning, and hands-on plugin UI approval remain open gates.

## Transmission V2 vertical slice

Transmission now uses a second dependency-free processor in `native/core`. It
ports the web receiver's level-matched pre/post saturation, repeated filter
stages, causal six-millisecond carrier displacement, deterministic dropouts and
interference, colored noise, generational loss, squelch transitions, fixed host
latency, and latency-aligned Mix control. The JUCE adapter retains embedded
tuning recordings and transport-aware search events.

The legacy fixed 860 x 540 tab grid has been replaced by a resizable receiver
surface with a live tuning scale, signal meter, clear Surface/Advanced modes,
six no-scroll advanced sections, protected Surface linking, direct-circuit
unlocking, input/mix/output controls, and sixteen reset-safe factory profiles.
The existing `TrnE` identity and parameter identifiers are preserved; new V2
parameters are appended and saved with `engineId=transmission` and
`schemaVersion=2`.

The portable Transmission test protects fixed latency, latency-aligned dry mix,
seed determinism, host-block-size invariance, bounded finite output, macro
influence, and a material clean/damaged difference. Audible comparison against
frozen web renders, smoothing under aggressive automation, DAW recall, and
hands-on UI approval remain open gates.

## Comms V2 vertical slice

Comms now uses a third dependency-free processor in `native/core`. Its five
hardware modes do not share one generic radio curve: Landline uses companded
quantization and carbon-granule modulation; Cellular uses predictive coding and
packet failures; Intercom emphasizes half-duplex gating and box resonance; PA
uses a wider driven horn and distant space; Alarm Panel adds a protected
two-tone generator. Signal-excited transducers, line aging, hum/hiss, early
reflections, room response, distance filtering, AGC, and final limiting complete
the path.

The previous editor-driven macro updates have been removed. Surface mapping now
runs in the processor even when the editor is closed, while Surface Link can be
disabled for direct hardware control. V2 appends the missing character,
distance, transducer, line-age, duplex, rattle, input, and mix parameters without
changing the existing `CmEg` identity or legacy parameter identifiers. The
resizable no-scroll console provides a live voice trace, hardware-channel
indicators, six focused Advanced panels, and sixteen reset-safe profiles.

The portable Comms test protects transparent dry mix, deterministic seeded
failures, host-block-size invariance, bounded finite output, macro influence,
and material separation between hardware modes. Audible comparison against
frozen web renders, parameter smoothing, DAW recall/automation, and hands-on UI
approval remain open gates.

## CD V2 vertical slice

CD now uses a fourth dependency-free processor in `native/core`. The legacy
VST read stereo input, averaged left and right into one sample, processed one
delay/history path, and wrote the identical result back to both outputs. V2
keeps disc position, sector failures, correction attempts, and tracking events
physically correlated while maintaining independent channel delay, history,
concealment, repeat, HF, compression, and output state. Radial scratches and
five alternate damage geometries, decoder correction, real history skips,
servo hunt, jitter, car-stereo levelling, stereo link/width, and direct Damage
and Skip triggers complete the first native model.

Surface macros are mapped in the audio processor and never rewrite the explicit
concealment mode. Repeat therefore stays Repeat until the user, host automation,
state recall, or a preset changes it. V2 appends advanced parameters without
changing the existing `CdEg` identity, and saves `engineId=cd` with
`schemaVersion=2`.

The resizable no-scroll optical deck provides Surface/Advanced views, live L/R
input and output meters, transport-state indicators, six focused advanced
panels, and sixteen reset-safe profiles. Portable tests protect exact reported
latency, latency-aligned stereo dry mix, separate channel histories, queued
manual skips, seed and host-block-size determinism, macro influence, and finite
bounded output. Audible web-reference comparison, parameter smoothing, DAW
recall/automation, and hands-on UI approval remain open gates.

## Developer builds

During V2 development, build plugins locally and test the generated `.vst3`
bundle directly. For Tape on Windows:

```powershell
cmake -S . -B build-tape -DLAE_BUILD_PLUGINS=ON -DBUILD_TESTING=OFF
cmake --build build-tape --config Release --target TapeEngine_VST3 --parallel 2
```

The bundle is generated at:

```text
build-tape/tape-vst3/TapeEngine_artefacts/Release/VST3/Tape Engine.vst3
```

CI uploads the current Tape, Transmission, Comms, and CD bundles together as
`lost-audio-v2-windows-dev-vst3` for build inspection. They are unsigned, are
not releases, and may be rejected by Windows
Smart App Control after download. A locally compiled bundle is the supported
development path until release signing is implemented.

`COPY_PLUGIN_AFTER_BUILD` remains disabled. Development builds must not silently
replace an installed release plugin; installation into a DAW scan path is an
explicit local QA step.

## Windows installer (deferred)

Installer work is parked until all individual VST3s meet the V2 quality bar.
The retained Inno Setup source is implementation work, not an active release
path. Before installers return, B&E Digital needs one trusted publisher identity
and an automated pipeline that signs every contained VST3 binary plus the setup
and uninstall executables.

The retained Tape V2 packaging definition uses Inno Setup 6.4.3. When the signed
release path is restored, it will request normal administrative elevation,
install the complete 64-bit bundle to the VST3 standard location at
`C:\Program Files\Common Files\VST3\Tape Engine.vst3`, and register a Windows
uninstall entry. It intentionally has no custom destination page, launcher, or
Start-menu shortcut.

To reproduce the package after building `TapeEngine_VST3` in Release mode:

```powershell
nuget install Tools.InnoSetup -Version 6.4.3 -OutputDirectory .tools -ExcludeVersion
.\scripts\build-tape-windows-installer.ps1 -BuildDirectory .\build-tape -OutputDirectory .\dist\windows -IsccPath .\.tools\Tools.InnoSetup\tools\ISCC.exe
```

The script validates the bundle and processor binary before compiling the installer, then emits the setup executable and a SHA-256 file. It is retained for later signing integration and must not currently be used to publish or hand off a test build.

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
