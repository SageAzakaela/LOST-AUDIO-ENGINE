# VST3 Three-Mode, Performance, and Telemetry Plan

Status: Approved direction; implementation plan, not a completion claim
Scope: The complete thirteen-product native fleet
Products: Brain Cruncher, Camcorder Engine, Cartridge Engine, CD Engine, Comms Engine, Conference Engine, Occlusion Engine, Open Mic Night, Tape Engine, Television Engine, Transmission Engine, Lost Audio Suite, and Lost Audio Sequencer

Implementation progress:

- **2026-08-30, Slice 1:** Added a shared in-block tempo-event schedule; CD now fires synced events at their sample offsets, supports deterministic per-step probability, uses adjustable force for manual triggers, defaults new states to canonical detailed parameters, materializes legacy factory macro recipes into explicit values, and presents persistent Simple/Advanced/Performer views with processor-driven disc/event telemetry.
- Slice 1 compiled as VST3 and passed CD core, tempo-sync, and canonical-state tests. Native Simple and Advanced layouts were visually inspected at the 1120 x 720 design size. Performer visual inspection, Ableton loading, audible QA, automation QA, and installation remain open.
- **2026-08-30, Transmission parity:** Replaced the four-macro default workflow with canonical receiver controls while preserving legacy session recipes. Added sample-accurate clocked carrier loss and tuning-search events, manual triggers, synced carrier motion, deterministic grid probability, an explicit output ceiling, and processor-derived carrier, loss, compression, noise, interference, squelch, tuning, limiter, level, and waveform telemetry.
- Transmission compiled as VST3 and standalone, passed core/tempo/adapter tests, and passed background visual QA for Simple, Advanced, and Performer at the 980 x 660 minimum size. Ableton loading, audible QA, automation QA, and installation remain open.
- **2026-08-30, Conference parity:** Materialized legacy platform macros into canonical codec, voice-band, packet, jitter-buffer, cleanup, and safety controls. Added sample-accurate clocked packet-concealment and captured robot-grain events, independent trigger and length grids, deterministic probability, manual triggers, event depth/strength, clocked jitter updates, and processor-derived packet, robot, jitter, suppression, AGC, comfort-noise, bandwidth-collapse, limiter, level, and waveform telemetry.
- Conference compiled as VST3 and standalone, passed core and adapter tests, and passed background visual QA for Simple, Advanced, and Performer at the 980 x 660 minimum size. Ableton loading, audible QA, automation QA, and installation remain open.
- **2026-08-30, Camcorder parity:** Resolved the legacy capture macros into canonical microphone, AGC, transport, converter, loss, movement, bed, and safety controls while retaining the exact default sound and existing session recipe. Added sample-accurate clocked capture loss, codec faults, and camera impacts, independent probabilities and event lengths, manual triggers, clocked flutter, explicit Wind and Camera Bed ownership, and processor-derived event, AGC, flutter, bed, limiter, level, and waveform telemetry.
- Camcorder compiled as VST3 and standalone, passed core/tempo/adapter tests including isolated embedded-bed and exact-default migration checks, and passed background visual QA for Simple, Advanced, and Performer at the 980 x 660 minimum size. Ableton loading, audible QA, automation QA, and installation remain open.
- **2026-08-30, Cartridge parity:** Replaced the macro-first surface with direct codec, word-length, memory-clock, DAC, address-wear, playback-body, and output controls while retaining legacy recipe migration and the exact default sound. Added a captured-history ROM stall/repeat path, bank/address-line foldback, independent clocked trigger, duration, and repeat-window grids, deterministic probability, manual ROM/bank/chip triggers, explicit chip-voice ownership, and processor-derived event progress, levels, and waveform telemetry.
- Cartridge compiled as VST3 and standalone, passed core and adapter tests including exact-default migration, conducted failure events, telemetry, and safety bounds, and passed background visual QA for Simple, Advanced, and Performer at the 980 x 660 minimum size. Ableton loading, audible QA, automation QA, and installation remain open.
- **2026-08-30, Open Mic Night parity:** Separated the Audience signal from the Venue Bed so crowd level remains audible and independently owned, then replaced the procedural audience stand-in with four selectable stereo crowd beds and separate applause/cheer reactions from the web tool. Beds are loudness-normalized and crossfade safely. Reactive mode follows input energy, recognizes phrase endings, and uses humanized reaction delay and recovery timing; Steady Ambience and Manual / Clocked prevent automatic reactions. Manual and explicitly clocked cheer/applause/chatter/heckle events remain available without defining the default personality. Replaced the macro-first surface with direct microphone, PA, monitor, room, crowd, feedback, and safety controls while retaining legacy recipe migration. Added processor-derived input-energy, audience-response, crowd, room, feedback, limiter, event, level, and waveform telemetry.
- Open Mic Night compiled as VST3 and standalone, passed core and adapter tests including independent audience/venue-bed routing, exact-default migration, conducted events, telemetry, block invariance, explicit feedback ownership, and bounded output, and passed background visual QA for Simple, Advanced, and Performer at the 980 x 660 minimum size. Ableton loading, audible QA, automation QA, and installation remain open.

## 1. Product intent

The native plugins remain device, space, and failure emulations first. Musical control is an additional way to conduct those models; it must not replace their strange, realistic, exaggerated, or destructive behavior.

Every focused engine will provide three views over one canonical plugin state:

- **Simple:** a small selection of the most useful real DSP parameters. Simple is not a hidden alternate engine and does not calculate an inaccessible second state.
- **Advanced:** the complete device-emulation model, including all physical, electrical, environmental, stochastic, and safety parameters.
- **Performer:** the complete controls relevant to live manipulation, plus triggering, host clock, MIDI, bounded variation, and optional protection against accidental chain-destroying states.

Changing views must never change the sound. Mode selection is editor navigation, not an automatable audio parameter.

## 2. Collection-wide rules

### 2.1 Canonical parameters replace macro-linked dual states

New patches and factory presets must use the real processor parameters as their authority. The current `macroLink` path must not remain the normal authoring workflow.

- Simple controls attach directly to canonical parameters.
- A device/profile selector may write a coherent group of canonical values, but the resulting values must immediately be visible in Advanced and Performer.
- No control may appear editable while a hidden mapping silently overrides it.
- New presets must not depend on `macroLink`.
- Existing parameter IDs remain registered for DAW/session compatibility.
- Existing sessions with legacy macro state must continue to render as before.
- Factory presets must be converted to explicit resolved values and compared against frozen renders before replacement.
- Legacy macro automation requires a documented compatibility path; it must not be silently disabled on state load.

Simple mode is therefore curated rather than macro-generated. It may contain eight excellent controls instead of four controls that invisibly move twenty others.

### 2.2 Presets are scoped and non-destructive

Factory content will be organized into:

- Device and Space profiles
- Condition and Damage profiles
- Performance profiles
- Experimental and Exaggerated profiles

Preset loading must support locks for Input, Output, Mix, Noise/Bed, Timing, and Safety. Existing destructive profiles remain available, but they are clearly categorized. Performance presets use conservative levels, finite event durations, and predictable clock behavior.

### 2.3 Performer protection preserves intentional ugliness

Performance protection targets accidents rather than aesthetics:

- non-finite samples and DC;
- uncontrolled output spikes and gain jumps;
- zippering from parameter changes;
- stuck repeats, feedback, or event states;
- retriggers faster than the processor can recover;
- unbounded accumulated noise or feedback energy.

It does not automatically remove hiss, hard edits, distortion, servo sounds, packet loss, rattle, feedback, or other intentional character. Protection is visible, adjustable, and defeatable. Neutral protection parameters must not alter existing sound.

### 2.4 Time has a shared `FREE / SYNC` contract

Every time-related control must be audited and classified rather than mechanically converted.

**Durations** may use milliseconds/seconds in Free and note values in Sync:

- delays, predelays, repeats, grains, packet windows, dropout lengths;
- feedback build/release, event recovery, gates, and envelopes;
- reverb or resonant decay where a musical duration is meaningful.

**Periodic motion** may use hertz in Free and cycles-per-note in Sync:

- wow, flutter, jitter modulation, drift, motion, scanning, and LFOs.

**Stochastic occurrence** may use events-per-second in Free and per-step probability in Sync:

- scratches, dropouts, packet loss, handling events, corruption, and tracking failures.

**Physical clocks** remain physically named and measured:

- tape speed, CD rotation, sample rate, bit depth, codec frame size, and other model-defining clocks are not relabeled as note values;
- triggers or modulation derived from those clocks may be synchronized separately.

Shared clock behavior must include straight, dotted, triplet, and bar divisions; host PPQ; sample-accurate in-block event offsets; transport start/stop; loop and seek detection; deterministic restart; optional swing, phase, and bounded humanization; and an explicit internal-clock fallback when the host provides no position.

### 2.5 Visual feedback is instrumentation

Every persistent visual element must be one of:

- **Measured:** derived from processed audio, such as peak, RMS, gain reduction, correlation, or spectral energy.
- **Processor state:** published by DSP, such as event phase, buffer fill, repeat position, dropout envelope, resonator energy, or safety engagement.
- **Control state:** an honest display of a canonical parameter or resolved model choice.
- **Atmospheric:** clearly background decoration that does not pretend to measure signal behavior.

The UI timer may interpolate or render telemetry, but it may not invent DSP activity. Displays must read resolved processor state rather than raw legacy macro values. A display remains present in Simple, Advanced, and Performer; changing modes must not replace truthful feedback with a different fiction.

Each processor will publish a small realtime-safe telemetry snapshot. Depending on the engine, it can contain:

- input/output peak and RMS;
- wet, bed/noise, and dry contribution levels;
- event active/type/progress/intensity;
- modulation phase and instantaneous displacement;
- gain reduction, limiter activity, and safety state;
- buffer, repeat, room, resonance, or feedback energy;
- current host division, step phase, and next trigger where applicable.

Telemetry is decimated and transferred without allocation, locks, file IO, or UI access on the audio thread.

## 3. Shared interface structure

Each focused engine uses a consistent frame:

- identity, preset browser, previous/next preset, and patch status;
- `SIMPLE / ADVANCED / PERFORMER` navigation;
- persistent device-specific instrument display and input/output meters;
- exact values, fine adjustment, reset, automation indication, and tooltips;
- visible Mix, Output, bypass, clipping, and safety state;
- no scrolling at the supported minimum laptop size.

The visual identity remains engine-specific. Shared behavior does not require thirteen identical faceplates.

## 4. Per-product plan

### 4.1 Television Engine

**Simple:** Set, speaker/cabinet, drive, compression, CRT Bed, tuner noise, faults, Mix, and Output as canonical controls.

**Advanced:** Full receiver filters, cabinet resonance/rattle, AGC, snow tone/crackle, hum, flyback, tuner drift, sync instability, power sag, bed routing, limiter, ceiling, and I/O.

**Performer:** Independent cabinet, CRT bed, reception-noise, electrical-noise, and fault levels; MIDI/automation-friendly layer controls; manual sync fault; clocked fault probability and duration; bounded noise ceiling; gain matching and output protection.

**Timing:** Sync-fault occurrence/duration and tuner-drift modulation can follow clock. Physical flyback and mains frequencies remain physical.

**Feedback:** Preserve the CRT display, but drive its trace from processed audio; show separate cabinet, CRT-bed, tuner-noise, and electrical contributions; publish sync-fault progress, AGC reduction, cabinet/rattle energy, and limiter activity. Scan lines may remain atmospheric.

### 4.2 CD Engine

**Simple:** Damage chance, damage depth, Skip chance, repeat size, correction, concealment, Mix, and Output.

**Advanced:** Geometry, error and scratch rates, burst/interpolation/repeat durations, tracking offset/rate, servo hunt/noise, rotation, jitter, HF loss, car compression, stereo behavior, and protection.

**Performer:** Manual and velocity-sensitive Damage/Skip pads; MIDI notes; Free, Clock, Pattern, Random, and Transient trigger sources; event probability, strength, length, jump distance, recovery, variation, swing, and phase; minimum retrigger and maximum-repeat protection.

**Timing:** Burst, repeat, tracking offset, interpolation, jitter motion, and event occurrence receive Free/Sync representations. Rotation remains a physical disc parameter. Clock events must occur at sample-accurate offsets rather than audio-block boundaries.

**Feedback:** Replace the mostly representational disc behavior with a readable event timeline showing upcoming ticks, scratch crossings, damage/concealment/repeat phases, history jump and loop region, servo activity, and correction result. Retain the optical-deck identity and L/R meters.

### 4.3 Tape Engine

**Simple:** Tape speed, drive, compression, head/tone, wow, flutter, hiss, dropout, mechanism layer, Mix, and Output.

**Advanced:** Complete filters, head bump, wow/flutter depth and rate, saturation, compression, hiss/hum, dropout scheduler and duration, SFX bank/mode/level, limiter, and ceiling.

**Performer:** Manual/MIDI dropout and splice events; clocked dropout density and length; synced wow/flutter phase options; mechanism triggers; bounded variation; event recovery and click-safe state changes.

**Timing:** Wow/flutter rate, dropout rate/duration, transport/mechanism events, and applicable SFX triggers.

**Feedback:** Reel rotation follows actual transport speed and host run state; show instantaneous wow/flutter displacement, dropout envelope/progress, saturation and compression activity, mechanism-bed contribution, and output protection. Reels must not spin merely because the editor timer is running.

### 4.4 Transmission Engine

**Simple:** Receiver profile, bandwidth, drive, connection damage, interference/noise, generations, Mix, and Output.

**Advanced:** Complete filters, compression/asymmetry/crush, carrier displacement, dropout, crackle/noise color, squelch, walkie behavior, tuning assets, passes, I/O, and protection.

**Performer:** Manual/MIDI dropout and tuning-search triggers; synced tuning cuts and sweeps; clocked dropout probability/duration; push-to-talk/squelch performance control; phase-locked carrier motion where desired.

**Timing:** Carrier/LFO motion, dropout occurrence/duration, walkie silence/click windows, tuning snippet/cut duration, and tuning trigger division.

**Feedback:** Receiver scale follows actual tuning/search progress; show carrier displacement, signal/squelch state, dropout envelope, noise/interference contribution, active tuning asset progress, generation count, and limiter state.

### 4.5 Comms Engine

**Simple:** Hardware mode, transducer, distance, bandwidth, drive, line age, noise, room/echo, Mix, and Output.

**Advanced:** Carbon/cellular/PA/intercom/alarm behaviors, compression, conversion and packet damage, duplex, enclosure/rattle, hum/hiss, echo, room, tone generator, and protection.

**Performer:** MIDI/push-to-talk gate, half-duplex performance control, alarm trigger, clocked packet events, synced echo/room timing, rhythmic line failures, and bounded transducer excitation.

**Timing:** Packet windows, half-duplex gates, echo delay/feedback, room duration, alarm cadence, and failure occurrence.

**Feedback:** Console trace is sourced from processed signal; indicate transmit/receive gate, codec/packet activity, alarm state, echo/room energy, AGC reduction, transducer/rattle excitation, and signal/noise balance.

### 4.6 Conference Engine

**Simple:** Platform, bandwidth, codec severity, dropouts, jitter, robotization, suppression, Mix, and Output.

**Advanced:** Codec frame and rate, packet loss/burstiness, concealment, repeat, jitter buffer, gate/suppression, AGC, buffer slip, bandwidth switching, comfort noise, stereo state, and protection.

**Performer:** Manual/MIDI packet failure, freeze, repeat, and robot triggers; clocked probability, duration, and target; rhythmic glitch patterns; bounded recovery; minimum retrigger and stuck-buffer protection.

**Timing:** Packet/repeat windows, jitter modulation, buffer slips, bandwidth collapse, suppression/recovery, and event occurrence.

**Feedback:** The call display becomes a packet/jitter-buffer monitor: decoded frames move through the buffer and visibly become lost, concealed, repeated, slipped, or recovered. Show actual buffer fill, packet-event progress, robot activity, voice gate, comfort-noise contribution, and L/R meters.

### 4.7 Camcorder Engine

**Simple:** Format, microphone, AGC, movement, corruption, wind, handling, camera bed, Mix, and Output.

**Advanced:** Filters, box tone, AGC speed/pump, clip/crush/conversion, dropout mode and duration, repeat, flutter, chirp, handling/rub, hiss, motor bleed, bed, and protection.

**Performer:** Manual/MIDI corruption, dropout, handling, and chirp triggers; clocked dropout/repeat behavior; synced flutter option; wind/handling envelope control; event probability and maximum duration.

**Timing:** AGC response, drop/repeat duration, corruption/drop occurrence, flutter, handling/rub events, and chirp cadence.

**Feedback:** Keep the viewfinder identity but publish AGC reduction, dropout/corruption progress, conversion hold state, wind/handling/motor/bed contributions, and actual input/output traces. Decorative recording overlays remain visually subordinate to measured state.

### 4.8 Cartridge Engine

**Simple:** Codec, speaker, quality, grit, noise, conversion, bleeps, ambience, Mix, and Output.

**Advanced:** Dither/noise shaping, bit/sample conversion, jitter, filters, emphasis, mu-law/block processing, saturation/edge, tracked noise/DC, hum/whine, complete bleep oscillator, micro-delay, reverb, limiter, and ceiling.

**Performer:** Manual/MIDI bleep pads; scale/key and pitch behavior; clocked bleep patterns and probability; synced micro-delay/reverb; clocked corruption blocks; safe artifact density and output protection.

**Timing:** Block duration, bleep occurrence/vibrato, micro-delay, reverb duration, jitter motion, and corruption events. Sample rate remains a physical conversion setting.

**Feedback:** Cartridge deck shows the actual codec block clock, held sample/bit-depth state, speaker activity, bleep note and envelope, sync phase, and separate noise/hum/whine energy instead of a generic active lamp.

### 4.9 Occlusion Engine

**Simple:** Material, construction, boundary amount, distance, source room, listener room, room return, Mix, and Output.

**Advanced:** Transmission filters/EQ, resonance, cavity, rattle/looseness, leak/tone, smear, predelay, room size/damping, limiter, ceiling, and I/O.

**Performer:** Manual/MIDI boundary-impact excitation; clocked rattle impulses; synced predelay and optional resonant decay; automatable source/listener movement; safe resonance-energy limit without removing sustained-drone capability.

**Timing:** Predelay, smear, rattle/impact occurrence, cavity decay, and room decay where the model exposes a meaningful duration.

**Feedback:** The boundary view shows measured energy in direct, transmitted, leak, and room paths; source/listener positions reflect canonical state; resonant and rattle energy animate the material itself; display predelay and room-tail progress. Wave traces must use signal/history telemetry rather than a timer-generated sine.

### 4.10 Open Mic Night

**Simple:** Microphone, venue, PA, selectable audience bed and behavior, proximity, drive, stage/crowd level, base energy, response, feedback amount/arm, Mix, and Output.

**Advanced:** Mic/PA tone and drive, monitor, feedback frequency/Q/delay/build/release, stage bleed, crowd bed, behavior, base energy, listening sensitivity, response, recovery, electrical noise, venue absorption/width/hum, safety, ceiling, and I/O.

**Performer:** Reactive/steady/manual audience selection, crowd and stage-event triggers, explicit optional crowd clocking, conducted feedback, clocked feedback delay/build/release, and explicit feedback-energy limits.

**Timing:** Feedback delay/build/release, room response, hot-mic gating, crowd events, and applicable stage/venue events.

**Feedback:** Stage routing becomes a live diagram of mic, monitor, PA, room, audience, and feedback-loop energy. Show feedback frequency/activity, build and release progress, safety intervention, bed/crowd contribution, and L/R meters. The stage must not imply signal where none exists.

### 4.11 Brain Cruncher

**Simple:** Crunch, Body, Bite, Space, Motion, Width, binaural amount, Mix, and Output.

**Advanced:** Individual resonant/comb behavior, excitation, rattle, smear/delay, drive, binaural ear spacing, pan, head size, width, gain, and ceiling. Existing broad controls become canonical audible controls rather than a linked hidden layer.

**Performer:** Manual/MIDI excitation, hold/freeze where technically safe, clocked motion and resonant impulses, synced smear/delay, bounded stereo motion, and resonance-energy protection.

**Timing:** Motion, comb/smear delay, resonant excitation/decay, and binaural modulation where applicable.

**Feedback:** Neural scope uses actual L/R phase relationship, stereo motion, resonator-bank energy, rattle state, excitation envelope, and output level. Timer-only neural activity is not presented as signal activity.

### 4.12 Lost Audio Suite

**Simple:** Clear serial chain, device/preset per slot, bypass, Mix, reorder, master I/O, and safety.

**Advanced:** Full canonical editor for the selected slot; complete chain state; per-slot timing, quality, and safety; master routing and gain structure. No generic six-knob abstraction may replace an engine's meaningful controls.

**Performer:** Scene recall, MIDI slot/event routing, global host clock, per-slot division/multiplier, momentary bypass/wet throws, chain fills, safe morphing, and parameter locks. User-assigned performance controllers with explicit destinations and min/max ranges are permitted; hidden factory macro states are not.

**Timing:** A shared clock service feeds eligible per-engine parameters without forcing all slots to one division. Slot events remain sample-aligned after latency compensation.

**Feedback:** Persistent left-to-right signal flow with per-slot input, output, wet contribution, activity/event state, bypass, and latency; selected-slot device telemetry; master gain reduction, CPU, topology gain, and safety. Reordering feedback must communicate the actual processing order.

### 4.13 Lost Audio Sequencer

**Simple:** Pattern length, division, swing, active steps, engine per step, probability, intensity, Mix, safe random, and master I/O.

**Advanced:** Full canonical selected-step engine state, gate and transition behavior, deterministic seed, internal/host clock, per-step locks, and complete safety/output controls.

**Performer:** Pattern banks and scene launch, MIDI triggering, fills, mutes, ratchets, ties, gate length, per-step microtiming, mutation amount, live record of supported controls, and deterministic restart/seek behavior.

**Timing:** The Sequencer owns the reference implementation of divisions, swing, phase, ratchets, gate, host loop/seek, internal BPM, and sample-accurate event offsets.

**Feedback:** Retain current-step and transport telemetry, then add sub-step phase, probability decision, fired/skipped result, ratchet/gate progress, active engine/event type, transition state, and per-step output contribution. The visual playhead must follow DSP clock state, not repaint cadence.

## 5. Implementation order

### Phase A: Freeze and audit

1. Capture versioned state files and deterministic reference renders for every factory preset.
2. Inventory each parameter as canonical, legacy macro, timing, trigger, safety, or UI-only.
3. Inventory every persistent visual element as measured, processor-state, control-state, or atmospheric.
4. Add parameter-influence tests for every exposed control and identify controls that are overridden or inert.

### Phase B: Shared foundations

1. Implement the shared three-view navigation and canonical-control components.
2. Add legacy macro/preset migration without changing existing-session renders.
3. Implement the shared tempo service and Free/Sync parameter representation.
4. Implement the realtime-safe telemetry snapshot and common meters/status components.
5. Implement shared smoothing, event watchdog, DC/peak safety, and panic behavior.

### Phase C: Pilot engines

1. Television Engine proves layer separation, noise control, persistent telemetry, and non-destructive presets.
2. CD Engine proves manual/MIDI/clock/pattern events, sample-accurate timing, event visualization, and repeat protection.
3. Complete listening and Ableton recall/automation QA before multiplying the architecture.

### Phase D: Time/event-heavy engines

Tape, Transmission, Conference, Camcorder, Cartridge, and Open Mic Night receive three modes, timing, triggers, and truthful telemetry using the pilot components.

### Phase E: Physical space and resonant engines

Occlusion, Brain Cruncher, and Comms receive path-energy/resonance telemetry and the relevant performance excitation or communication controls.

Occlusion Engine implementation status (2026-08-30): complete for automated QA. New sessions use direct canonical controls while legacy macro sessions are materialised without changing their mapped sound. The cavity now uses bounded resonant feedback; source and listener rooms remain distinct; Simple, Advanced, and Performer share processor-derived body/room/leak/rattle/limiter telemetry and an output trace. Performer adds manual or host-clock boundary excitation and hardware strikes plus free/clocked stereo motion. Core and adapter tests cover migration, parameter influence, conducted events, telemetry, block invariance, and bounded output; DAW listening/recall QA remains pending with the fleet pass.

### Phase F: Meta products

Suite adopts the completed individual-engine interfaces and clock contracts. Sequencer then becomes the authoritative multi-engine performance host rather than maintaining divergent simplified mappings.

## 6. Release gates

An engine does not complete this pass until:

- switching Simple, Advanced, and Performer produces a null audio difference;
- all three modes show the same underlying values and persistent telemetry;
- every new-patch and converted factory preset uses canonical parameters;
- legacy sessions and automation load without an unexplained sonic change;
- every visible non-atmospheric element is traceable to measured or processor state;
- every eligible time parameter works in Free and Sync and survives tempo changes, stop/start, loop, seek, offline render, and state recall;
- trigger events are sample-accurate within the processing block;
- automation and preset changes are smoothed where discontinuities would be accidental;
- protection has tests for non-finite output, DC, ceiling, stuck states, and excessive retriggering;
- protection at neutral settings does not alter the reference render;
- mono/stereo, sample-rate, block-size, deterministic seed, pluginval, Ableton scan/load, automation, save/reopen, duplicate-instance, and removal tests pass;
- a listening pass approves subtle, realistic, exaggerated, destructive, and performance presets separately.

## 7. Immediate next slice

Implement only the shared foundations required by Television and CD, then complete those two pilots before modifying the remaining engines. This keeps the collection-wide architecture honest while protecting the existing fleet from a broad speculative rewrite.
