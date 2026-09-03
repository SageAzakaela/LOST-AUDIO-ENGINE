# Native VST3 status and build guide

## Current status

The repository contains thirteen JUCE projects. They build VST3 and Standalone targets, preserve existing plugin identities, and contain useful DSP/UI work. They are not currently the sonic authority and must not be described as equivalent to the current web workstation until listening QA establishes parity.

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

## Conference V2 vertical slice

Conference now uses a fifth dependency-free processor in `native/core`. The
legacy plugin's continuously modulated delay, independent random volume cuts,
and forced mono output have been replaced by a linked stereo speech transport:
codec-frame scheduling, burst-memory packet loss, four packet-loss concealment
strategies, recovery crossfades, jitter-buffer slips, temporary bandwidth
collapse, repeated decoded-speech grains, voice activity suppression, AGC, and
comfort noise. The failure clock is shared while each channel retains its own
history, filtering, codec, and concealment samples.

Surface mapping runs in the audio processor even when the editor is closed.
Surface Link can be disabled for direct control of the advanced codec state.
V2 appends burst, suppression, AGC, buffer, comfort-noise, input, and Mix
parameters without changing the existing `CfEg` identity or legacy parameter
indices. State is saved with `engineId=conference` and `schemaVersion=2`.

The fixed stock-tab editor has been replaced by a resizable no-scroll call
console with Surface/Advanced views, a live decoded-speech trace, L/R meters,
packet/PLC/buffer indicators, six focused Advanced panels, and sixteen
reset-safe profiles. Portable tests protect exact 20 ms reported latency,
latency-aligned dry mix, deterministic host-block-independent failures,
separate stereo content, macro influence, and finite bounded output. Audible
web-reference comparison, automation smoothing, DAW recall, and hands-on UI
approval remain open gates.

## Camcorder V2 vertical slice

Camcorder now uses a sixth dependency-free processor in `native/core`. The
legacy native path omitted the web model's recording format, microphone model,
flutter, AGC pumping, and motor bleed; summed stereo to mono; and let editor
macros silently toggle Wind and rewrite concealment. V2 restores five format
families and five physical camera microphones while keeping independent stereo
capture histories under linked camera events.

Wind, body contact, and corruption now use separate deterministic random
streams. Corruption cannot change Wind scheduling, Wind Level is inert while
the explicit Wind switch is off, and disabling Wind ends its bed immediately.
Rare format faults, capture dropouts, analog/digital transport motion, camera
AGC, body resonance, handling thumps, contact scrape, mic hiss, and mechanism
bleed each have distinct paths. Surface mapping runs in the processor but never
owns Wind or the selected Hold/Mute/Interpolate/Repeat strategy.

The stock four-tab grid has been replaced by a resizable no-scroll camera body
with a live REC viewfinder, L/R meters, separate Wind/body/dropout/codec lamps,
Surface/Advanced views, six focused Advanced panels, and sixteen reset-safe
scene profiles. The `CcEg` identity and all V1 parameter indices remain intact;
new controls are appended and state saves as `engineId=camcorder` with
`schemaVersion=2`. Portable tests compile and cover fixed 3 ms latency,
latency-aligned dry mix, deterministic host-block behavior, stereo separation,
Wind ownership and isolation, macro range, and bounded output. On the current
Windows development machine, Smart App Control blocked the newly generated test
executable from running; CI execution and later consolidated listening/DAW/UI
approval therefore remain open gates.

## Cartridge V2 vertical slice

Cartridge now uses a seventh dependency-free processor in `native/core`. The
legacy plugin collapsed stereo to mono, exposed no real codec or playback-device
choice, and treated its procedural bleeps as an ambient random layer. V2 keeps
independent left/right converter, predictor, filter, speaker, delay, and room
state while linking only the hardware clock and optional chip-note events.

PCM, DPCM, adaptive ADPCM, BRR-style prediction, and mu-law companding are now
distinct memory paths. Direct, handheld, television, arcade cabinet, and PC
speaker models provide separate physical playback responses. Dither, error
shaping, pre-emphasis, block resets, bounded hum/whine/noise, DAC edge,
saturation, body delay, and short enclosure reflections complete the device
path. Surface mapping runs in the processor, with a direct-detail unlock for
advanced work.

Chip Voice is explicitly user-owned: no surface macro can enable it, disabling
it makes its trigger controls inert, and only the clearly named Chip Voice Demo
factory profile arms it. Transient, clock, and hybrid triggering; five wave
sources; and four musical scales replace the legacy uncontrolled bleeps, with a
hard-bounded injection level before cartridge processing.

The resizable no-scroll editor uses a cartridge-bay display, live stereo meters,
memory/playback readouts, Surface/Advanced views, six focused Advanced panels,
and sixteen reset-safe profiles. The `CrEg` identity and all V1 parameter
indices remain intact; V2 parameters are appended and state saves as
`engineId=cartridge` with `schemaVersion=2`. Portable tests protect transparent
dry mix, deterministic host-block behavior, stereo separation, seed influence,
explicit chip ownership, macro range, and finite bounded output. The VST3
bundle builds locally with JUCE's optional auto-manifest step disabled because
Windows Application Control blocks newly generated helper executables. The
standalone executable was likewise blocked from launching, so DAW scanning,
audible comparison, automation smoothing, recall, and hands-on UI approval
remain consolidated QA gates.

## Television V2 vertical slice

Television now uses an eighth dependency-free processor in `native/core`. The
web reference's repeated speaker filters, broadcast levelling, static texture,
60 Hz leakage, flyback tone, and captured CRT bed remain recognizable, but the
native model now distinguishes the television from its incoming signal.
Portable, console, broadcast-monitor, kitchen, and motel sets have materially
different bandwidth, cabinet resonance, and rattle behavior; baseband, antenna,
cable, and detuned reception change tuner noise and instability independently.

The V1 adapter allowed editor macros to rewrite precision controls, then
overwrote parts of that mapping in the processor. It also advanced the mono CRT
bed separately inside each channel loop. V2 maps Surface controls entirely in
the processor, offers a direct-detail unlock, advances the bed once per sample,
and injects it through the television path while retaining latency-free dry
mix. Chip-like random damage has been replaced with bounded reception snow,
sparse tuner crackle, slow tuner drift, linked sync faults, signal-excited
cabinet rattle, power sag, realistic flyback, and a protected octave-down
flyback component for exaggerated states.

The CRT bed remains explicitly armed and is inert when disabled. The resizable
no-scroll editor provides a live scanlined CRT trace, stereo meters, reception,
bed, snow, and sync indicators, Surface/Advanced views, six focused Advanced
panels, and sixteen reset-safe profiles. The existing `TvEg` identity and all
V1 parameter indices remain intact; V2 controls are appended and state saves as
`engineId=television` with `schemaVersion=2`. Portable tests protect transparent
dry mix, deterministic host-block behavior, stereo separation, silent disabled
textures, material model/reception separation, and bounded output. The VST3
bundle builds locally; Windows Application Control blocked the fresh standalone
executable, leaving DAW scanning, audible comparison, automation smoothing,
recall, and hands-on UI approval for consolidated QA.

## Occlusion V2 vertical slice

Occlusion now uses a ninth dependency-free processor in `native/core`. Eight
materials and five construction types define distinct transmission bandwidth,
absorption, body modes, cavity behavior, smear, leakage, damping, and
signal-excited rattle. The effect therefore models a source, a physical
boundary, and a listener space instead of reducing occlusion to a low-pass
filter and delay. Metal, glass, hollow framing, and loose panels retain useful
resonant exaggeration while brick and concrete remain dense and controlled.

Surface mapping runs in the processor and can be unlocked for direct detail.
The resizable no-scroll interface visualizes source-to-boundary-to-listener
transmission, stereo levels, material thickness, and live hardware excitation;
six Advanced panels expose the full tone, body, room, leak, precision, and
safety paths. Sixteen reset-safe boundary profiles are included. The existing
`OcEg` identity and all V1 parameter indices remain intact; Metal and Concrete
are appended to the material list, V2 parameters are appended, and state saves
as `engineId=occlusion` with `schemaVersion=2`. Portable tests protect dry-mix
transparency, deterministic host-block behavior, stereo separation, material
separation, and bounded output. Audible comparison, DAW scanning, automation,
recall, and hands-on UI approval remain consolidated QA gates.

## Open Mic Night V2 vertical slice

Open Mic Night completes the ten-plugin individual V2 fleet with a tenth
dependency-free processor in `native/core`. It now models a complete live
sound scene rather than a mono feedback delay: five microphones, five PA
systems, six venues, proximity and preamp behavior, monitor spill, early and
late venue returns, audience absorption, recorded crowd activity, electrical
noise, and a signal-fed feedback loop. Stereo input remains stereo throughout
the mic, PA, room, and audience paths.

Feedback is never owned or silently enabled by a macro or factory profile. It
has a dedicated automatable arm, amount, build, release, frequency, Q, delay,
and tone. The internal loop is saturated and bounded before a separate output
safety ceiling; all sixteen factory profiles load disarmed, including the two
clearly labeled Howl Ready setups. The developer-QA build embeds four selectable
audience beds plus separate applause and cheer reactions from the web tool,
resamples and loudness-normalizes them during prepare, and crossfades bed changes
and loop seams. Reactive mode follows input energy and responds after phrase
endings with a humanized delay and recovery window; Steady Ambience and Manual /
Clocked keep automatic behavior explicitly disabled. Public packaging still
requires explicit provenance and distribution-rights confirmation.

The resizable no-scroll interface visualizes microphone, wedges, PA, venue,
audience, feedback-loop activity, and active safety reduction. Surface controls
stay simple, while Advanced exposes the full physical and safety model. The
existing `OmNt` identity and all nine V1 parameter indices remain intact; V2
parameters are appended and state saves as `engineId=open-mic-night` with
`schemaVersion=4`. Portable tests protect dry-mix transparency, deterministic
host-block behavior, stereo separation, venue/device differentiation, explicit
feedback ownership, and bounded maximum-feedback output. Audible comparison,
DAW scanning, automation, recall, and hands-on UI approval remain consolidated
QA gates.

## Lost Audio Suite V1 vertical slice

Lost Audio Suite by B&E Digital hosts every shared engine in one native `LaSu` VST3. Six
physical slots each retain independent processor history and can hold repeated
instances of the same engine; a separate permutation defines serial order, so
reordering does not exchange slot settings. The rack supports drag reordering
plus explicit up/down controls, per-slot bypass and Mix, two primary macros,
model selection, and two bipolar assignment paths from each of two global
macros. The selected slot receives a large focused inspector rather than
compressing the complete family into a tiny control grid.

The core reports a fixed 120 ms worst-case latency at 48 kHz and pads each
active topology to match it. Type, bypass, and order changes fade to silence,
switch at the zero boundary, and recover over a short ramp. Twelve factory
chains cover subtle use, voice devices, found footage, spatial obstruction,
conference damage, game media, CRT playback, and deliberately exaggerated
failure. Every chain loads Open Mic feedback disarmed; the core additionally
requires an observed off state after insertion or recall before an arm can
become active. Television slots receive the same embedded CRT bed used by the
individual Television Engine rather than a reduced bedless core path.

Portable tests protect fixed host latency, deterministic block behavior,
stereo retention, audible serial order, repeated-engine independence,
click-safe topology changes, and bounded maximum-chain output. The VST3 and
Standalone wrappers compile on Windows. Pixel-level UI review, DAW scanning,
audible preset approval, automation smoothing, session recall, preset-file
interchange, formal plugin validation, and quality/oversampling remain V1
approval gates rather than claimed-complete work.

## Lost Audio Sequencer V1 vertical slice

Lost Audio Sequencer is the rhythmic companion to Lost Audio Suite. Its 16-step
host-synced grid chooses one shared device engine per step, with independent
Character, Damage, Probability, Mix, and Device Model values. Pattern length,
straight/triplet/dotted divisions, swing, input/output trim, master mix, safety,
and ceiling are host-automatable. A standalone Audition clock allows pattern
work without a DAW transport.

Six guarded factory patterns cover weathering, broken broadcast, CD failure,
continuous wall traversal, failed calls, and mixed machine damage. Safe Random
limits density, damage, wet level, and probability ranges. Open Mic feedback is
never armed by the sequencer, and the shared suite limiter remains last in the
signal path. Television steps receive the embedded CRT bed.

The timing core is tested for host-grid wrap, swing, negative pre-roll, and
deterministic probability. Wrapper tests cover editor creation at default and
minimum size, state recall, complete parameter exposure, and finite stopped-
transport output. The VST3 and Standalone compile on Windows and the no-scroll
surface has received pixel-level QA. Ableton scanning, audible preset approval,
automation recording, transport seek/loop stress, and formal plugin validation
remain hands-on QA gates.

## Developer builds

During V2 development, build plugins locally and test the generated `.vst3`
bundles directly. To build the complete B&E Digital fleet on Windows:

```powershell
cmake -S . -B build-tape-dev -DLAE_BUILD_PLUGINS=ON -DBUILD_TESTING=ON
cmake --build build-tape-dev --config Release --parallel 2
ctest --test-dir build-tape-dev -C Release --output-on-failure
```

The bundle is generated at:

```text
build-tape-dev/tape-vst3/TapeEngine_artefacts/Release/VST3/Tape Engine.vst3
```

CI uploads all eleven individual B&E Digital bundles, Lost Audio Suite, and Lost
Audio Sequencer as `be-digital-vst3-windows-dev` for build inspection. They are unsigned, are
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

The visible publisher for every plugin is **B&E Digital**. The all-in-one
product remains **Lost Audio Suite**; individual products retain their own
Engine names. The legacy `LsAu` manufacturer code and `com.lostaudio.*` bundle
IDs remain deliberately stable because changing them can break existing DAW
sessions. These four-character plugin codes and bundle IDs are
compatibility-sensitive: changing one can make a DAW treat an update as a
different plugin; reusing one can make two plugins replace each other.

| Project | Product | Code | Bundle ID |
| --- | --- | --- | --- |
| `brain-cruncher-vst3` | Brain Cruncher | `BrCr` | `com.bedigital.braincruncher` |
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
| `suite-vst3` | Lost Audio Suite | `LaSu` | `com.bedigital.lostaudiosuite` |
| `sequencer-vst3` | Lost Audio Sequencer | `LaSq` | `com.bedigital.lostaudiosequencer` |

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
