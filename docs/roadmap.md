# Lost Audio Engine product roadmap

Status: working product and engineering direction
Updated: 2026-08-27

## Product promise

Lost Audio Engine should become the definitive creative toolkit for believable recorded-device and transmission degradation:

- convincing enough for sound designers who care about the physical device and signal path;
- immediate enough that a new user can choose a device, turn a few meaningful controls, and get a good result;
- deep enough for advanced sound design, automation, destructive artifacts, and custom presets;
- consistent across the web app, individual plugins, the Suite, and the Sequencer;
- dependable enough for real sessions, saved projects, automation, and cross-platform releases.

The web implementation is the current sonic reference. A visual overhaul must not disguise weaker native processing.

## Product family

### Lost Audio Engine Web

The free browser workstation: upload, audition, chain, automate, and export. It remains the easiest way to discover the collection and should also become the clearest demonstration of every engine.

### Device Engines

Focused individual plugins for Transmission, Tape, Television, Cartridge, CD, Comms, Conference, Camcorder, Occlusion, and Open Mic Night. Each plugin should feel like its own object while sharing one interaction grammar and one DSP implementation with the rest of the family.

### Approved expansion direction

The next category expansion should prioritize space emulation. **Ductwork** and
**Water Boundary** are the first-priority space engines. Preferred later device
engines are **Dictaphone**, **Surveillance**, **Lossy Media**, and
**Turntable**. Other brainstormed spaces and devices remain stretch goals.
Their modeling boundaries and candidate controls are preserved in the
[expansion priorities](expansion-ideas.md).

### Lost Audio Suite (working name)

One native plugin containing the complete effect collection as reorderable slots. The first release should prioritize a fast serial signal chain; parallel lanes and more elaborate routing can follow after the core is stable.

### Lost Audio Sequencer (working name)

A tempo-synced multi-effect performance instrument inspired by the immediacy of glitch sequencers. It must have its own layout and identity rather than imitate another product.

## Non-negotiable architecture

### 1. Freeze and measure the web reference

Before replacing DSP, capture deterministic reference renders from the current web engines:

- clean voice, sung voice, full music, drums/transients, ambience, silence, and stereo material;
- representative subtle, authentic, damaged, and extreme presets;
- 44.1, 48, and 96 kHz where the platform supports them;
- fixed random seeds for dropouts, jitter, noise, crackle, and mechanical events;
- dry and loudness-matched processed outputs.

The current web master chain defaults to mono output, 0.25 gain, limiting, and soft clipping. Comparison tests must separate those global differences from the device DSP itself.

### 2. One portable DSP core

Move device processing into a reusable, realtime-safe C++ library with no JUCE UI dependency. Use it from:

- each individual JUCE plugin;
- Lost Audio Suite slots;
- Lost Audio Sequencer lanes;
- a WebAssembly AudioWorklet adapter for the browser.

Keep the existing JavaScript/Web Audio implementation available as `web-reference-v1` until the shared core passes listening and render comparisons. Do not delete the reference merely because the new code compiles.

### 3. One parameter and preset schema

Parameters, ranges, units, defaults, macro mappings, preset metadata, and migration rules should be defined once and generated/adapted for web and JUCE.

Every preset should be able to record:

- engine and schema version;
- device family or signal path;
- era and condition;
- intended source material;
- intensity category;
- evidence label: `measured`, `reference-informed`, `plausible`, or `stylized`;
- deterministic seed when random behavior is part of the sound.

### 4. Shared UI system, individual art direction

Build reusable JUCE components for preset browsing, knobs, switches, meters, tooltips, tabs/panels, resizing, A/B, undo/redo, gain matching, and accessibility. Individual plugins can supply their own faceplate, palette, typography, markings, and device-specific visualization without copying interaction code ten times.

## UX model

### Surface view

The default view should answer “what device is this, how damaged is it, and how much do I want?” without exposing a laboratory panel.

Common surface controls:

- device/profile selector;
- four or five engine-specific macro controls;
- Mix, Input, and Output;
- automatic loudness matching toggle;
- large preset browser with categories and search;
- previous/next preset, favorite, A/B, undo/redo, and safe randomize;
- input/output meters and clipping indication;
- clear bypass and quality mode.

Macro names should describe audible or physical outcomes—such as Signal, Condition, Mechanics, Distance, Tracking, and Environment—not implementation details.

### Advanced view

Advanced controls live in stable sections such as Tone, Mechanics, Failure, Environment, Modulation, and Output. Controls remain automatable and explain both what they do and what physical behavior they represent.

Randomization must support parameter locks and bounded intensity so it can be useful rather than merely chaotic.

### Visual direction

The family should feel like related recovered equipment, not ten identical dark JUCE panels:

- consistent header, preset, metering, navigation, and control behavior;
- distinctive materials, wear, labeling, status displays, and motion for each device family;
- restrained texture that does not reduce legibility;
- scalable/resizable layouts and high-DPI assets;
- color never used as the only state indicator;
- no tiny horizontal mega-grid as the primary Suite interaction.

## Web V2

Replace the current all-modules-across layout with a workstation structure:

- left: compact, reorderable signal-chain slots;
- center: the selected engine’s Surface or Advanced panel;
- right: preset browser, output controls, meters, and contextual help;
- bottom: waveform/transport for normal mode or a dedicated automation editor;
- clear audition A/B between dry, current, and previous states;
- import/export for user presets and complete sessions;
- autosaved recovery without silently transmitting audio or presets;
- responsive layout with a deliberately reduced mobile workflow.

Preserve the existing strengths: deterministic offline export, realtime preview using the same graph, module ordering, master/user presets, and automation.

## Individual plugin V2 standard

Every Device Engine must ship with:

- shared-core DSP at approved web-reference parity;
- resizable Surface and Advanced views;
- 20 or more curated presets where the engine supports meaningful variety;
- useful subtle presets, not only broken/extreme sounds;
- wet/dry mix, input/output gain, loudness matching, meters, bypass, A/B, and undo/redo;
- parameter smoothing and click-free state changes;
- deterministic or explicitly controllable random behavior;
- correct mono/stereo handling and saved-state migration;
- tooltips and a concise embedded guide;
- verified unique plugin identity across every format and platform.

Open Mic Night is a scene/performance processor rather than a pure recorded-device model. Keep it in the family, but give it a clearly labeled category and add it to the Suite only after its feedback behavior and embedded assets pass the same safety and rights audit.

## Lost Audio Suite V1

Core workflow:

- 4–8 reorderable effect slots;
- any Device Engine can occupy a slot, including repeated engines;
- per-slot bypass, Mix, preset, and two assignable macros;
- global input/output, gain matching, meters, limiter, and oversampling/quality mode;
- chain presets and per-slot presets;
- global macro assignments with min/max ranges and polarity;
- drag-to-reorder with click-free transitions;
- CPU display and a safe-quality fallback.

Later candidates: parallel lanes, crossover routing, mid/side routing, sidechain control, and modulation. These should not delay a stable serial-chain V1.

## Lost Audio Sequencer V1

Core workflow:

- 16/32-step patterns synchronized to host tempo and transport;
- effect lanes that can trigger an engine, bypass it, or change its Mix;
- per-step macro locks, probability, gate length, ratchets/repeats, and intensity;
- pattern banks, scene changes, swing, rate divisions, triplets, and host automation;
- controlled randomization with locks, seed recall, undo, and mutation amount;
- audition while stopped and deterministic playback when the host restarts;
- smoothing/crossfades that prevent clicks when states change;
- a global safety limiter that is visible and defeatable;
- complete state recall inside a DAW project.

The current web automation system is useful prior art, but the Sequencer should expose musical step behavior directly rather than requiring users to draw every enable/wet lane by hand.

## Device-emulation quality bar

Each engine needs a short model document defining its signal path and claims. Separate:

- **evidence:** measured curves, reference recordings, published standards, known codec/sample-rate behavior, and identifiable hardware traits;
- **interpretation:** reasonable modeling choices used where evidence is incomplete;
- **stylization:** intentionally exaggerated artifacts designed for storytelling or music production.

An “authentic” preset must name the device class and behaviors it models. Brand/model-specific claims require real supporting reference material. Creative presets can be wilder, but should be labeled accordingly.

## QA and release gates

### DSP tests

- deterministic offline golden renders;
- silence, impulse, sine sweep, noise, voice, music, and transient fixtures;
- peak, DC, loudness, spectral-envelope, stereo-correlation, and non-finite-sample checks;
- sample-rate, block-size, mono/stereo, bypass, wet/dry, and parameter-edge matrices;
- state save/restore and preset-schema migrations;
- no allocations, locks, file IO, or unbounded work on the audio thread;
- no zipper noise or unsafe discontinuities during automation.

### Plugin tests

- pluginval at a strictness level chosen and recorded by the project;
- Standalone and VST3 validation on Windows and Linux;
- VST3 and AU validation on macOS if AU is shipped;
- REAPER plus at least one additional host per operating system;
- install, rescan, load, automate, save, reopen, duplicate instance, change sample rate, and remove-instance smoke tests;
- Intel/Apple Silicon and signing/notarization decisions documented for macOS.

### Listening tests

Numerical similarity is not enough. Every port needs level-matched blind comparisons against the web reference and a short rubric covering identity, intelligibility, motion, realism, harshness, noise behavior, and usefulness at subtle settings.

## Phased delivery

### Phase 0 — Preserve, organize, and publish the reference

1. Archive and hash the current itch artifacts. (Local snapshot and manifest captured 2026-08-27.)
2. Recover the actual corrected Camcorder plugin ID before editing it. (Recovered as `CcEg` and reconciled in source; rebuilt-ID validation remains.)
3. Organize source documentation, remove generated release ZIPs from the source tree, and publish an honest architecture/QA map.
4. Establish dependency-free repository validation, deterministic processor tests, parity fixtures, and continuous validation.
5. Freeze web-reference renders and preset snapshots.
6. Decide the source/JUCE licensing path and audit bundled audio assets.

Exit: the current web reference is understandable, reproducible, and ready to be exercised on additional platforms without losing its lineage.

### Phase 1 — Linux and broader platform/device integration

1. Validate Chromium/Chrome on Linux x86_64, then Firefox and Linux ARM64 where hardware is available.
2. Exercise PipeWire, PulseAudio compatibility, built-in output, USB interfaces, device changes, and common sample rates.
3. Validate macOS and expand the Windows browser/device matrix where hardware is available.
4. Run the complete web workflow: decode, rack, presets, automation, mastering, long sessions, repeated file changes, and WAV export.
5. Document verified, experimental, unsupported, and untested combinations with honest fallbacks.

Exit: the web reference has evidence-backed Windows/Linux support, platform failures are reproducible, and the next native architecture is not being designed around one machine.

### Phase 2 — Native foundation and Transmission vertical slice

1. Pin JUCE and establish clean cross-platform CMake/CI builds.
2. Build the shared parameter/preset schema and migration system.
3. Implement a portable realtime-safe DSP core plus JUCE and WebAssembly/AudioWorklet adapters.
4. Port the complete web Transmission graph, including richer walkie/tuning behavior.
5. Create reusable Surface/Advanced UI components while retaining distinct device art direction.
6. Run parity, plugin validation, DAW recall/automation smoke tests, and level-matched listening.

Exit: one engine proves the complete cross-platform architecture and quality bar before it is multiplied.

### Phase 3 — Core device fleet

Port and validate in families:

1. Tape;
2. Comms and Conference;
3. Camcorder;
4. CD and Cartridge;
5. Television and Occlusion;
6. Open Mic Night after feedback safety and asset review.

Exit: every individual plugin meets V2 standard and can be hosted in the Suite.

### Phase 4 — Lost Audio Suite V1

Build the all-in-one native plugin from the validated shared engines: reorderable slots, fast serial routing, useful factory chains, session/preset interchange, and a cross-platform release pipeline. Parallel routing can follow after the serial workflow is stable.

Exit: the web reference and native Suite use the same approved models, parameter schema, migrations, and presets.

### Phase 5 — Lost Audio Sequencer V1

Build the musical step workflow on top of the already validated core, state model, smoothing, and Suite routing.

Exit: deterministic, host-synced patterns survive save/reopen and pass the DAW matrix.

## Current audit snapshot

- Ten independent JUCE plugin projects use duplicated editor/control code and fixed non-resizable windows.
- A root CMake entry point, deterministic parity-fixture tooling, dependency-free repository validator, and source-level CI now exist. JUCE remains unpinned; plugin validation, clean native rebuild proof, and native release automation are still absent.
- The web workstation builds nine engines from their standalone graph modules and supports a drag-and-drop left-to-right rack, focused inspectors, realtime preview, deterministic offline export, user/master presets, and four-layer automation.
- The web has 99 factory presets across the nine shared engines; the corresponding VSTs have 50.
- Transmission has 16 web presets versus 6 VST presets and web controls that the native parameter surface does not currently expose.
- The web Suite does not currently include Open Mic Night.
- The selected-device workstation, mastering drawer, automation drawer, walkthrough, laptop layout, deliberate mobile boundary, and protected rack presets are implemented. Cross-platform/device evidence and a formal reference-render freeze remain outstanding.

## Immediate implementation order

1. Publish the organized web reference, documentation, tests, issue forms, and CI.
2. Complete Linux and broader platform/device integration using the matrix in [platform-integration.md](platform-integration.md).
3. Freeze deterministic web reference renders and approve the source/JUCE/audio-asset licensing path.
4. Pin JUCE and build Transmission V2 as the shared-core vertical slice.
5. Approve sound, UX, automation, and recall before multiplying the pattern across the fleet.
6. Build the Suite from validated engines; build Sequencer behavior only on the same validated core.
