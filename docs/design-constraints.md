# Lost Audio Engine — Web Workstation Design Constraints

Status: Draft 0.1 for Azakaela's review
Scope: Web suite first. These constraints may later inform the VST Suite, but do not define the individual VST redesigns yet.

This document separates constraints explicitly established by Azakaela from proposed constraints that still need approval. No new interface direction should be treated as approved merely because it appears here.

## 1. Product identity — locked

- LOST is a live, modular device-emulation playground.
- It is for making audio sound old, strange, damaged, distant, unstable, recorded, transmitted, or physically compromised.
- Its depth is part of the appeal. Users are allowed to encounter multiple engines and learn what they do.
- The central pleasure is touching a signal and hearing it change immediately.
- The suite is not a questionnaire, onboarding wizard, story compiler, simplified consumer filter, or linear task funnel.
- Loading audio and hearing it must not be gated behind explanatory steps.

## 2. Primary workflow — locked

The default loop is:

1. Load audio.
2. Play or loop it.
3. Drag devices from a library into a left-to-right rack.
4. Manipulate the devices while audio is playing.
5. Reorder, bypass, remove, or replace devices freely.
6. Watch and hear the signal change.
7. Save a rack preset or export the result.

There is no required order beyond the signal path itself.

## 3. Spatial model — locked

- Device library on the left.
- Signal rack in the main workspace.
- Signal travels visibly from left to right.
- Devices enter the rack by drag and drop, with an accessible click-to-add equivalent.
- Rack order is the processing order.
- Reordering must feel physical and immediate, with a clear insertion preview.
- Empty rack state is a live dry signal, not an onboarding screen.
- Playback and signal feedback remain visible while the rack is being edited.

## 4. Device surfaces — locked

- A device should resemble either an analog guitar pedal or a compact rack module, whichever better fits its behavior.
- Devices are instruments, not generic dashboard cards.
- Their silhouettes, faceplates, control groupings, meters, lights, labels, and wear should communicate their function.
- Each device needs a distinct identity without becoming unreadable or visually unrelated to the suite.
- Surface controls expose the parameters most useful for live play.
- Advanced controls may expand, flip, slide out, or open a focused editor without replacing the rack.
- Enabled, bypassed, selected, clipping, loading, and modified states must be unmistakable.

## 5. Control feel and feedback — locked

- Legibility is functional: ordinary interface text must remain readable at normal desktop scale, with larger exact values and hierarchy for critical state.
- Controls must respond while audio is playing whenever the DSP permits it.
- Knobs, sliders, switches, buttons, and patch controls need visible movement and immediate sonic response.
- A control must show both its physical position and an exact readable value.
- Changes need tactile feedback through motion, light, meter response, or a short state readout—not decorative animation unrelated to the signal.
- Parameter changes must be smoothed where necessary to prevent clicks, discontinuities, or accidental blasts.
- Bypass and removal must not create uncontrolled transients.

## 6. Signal visualization — locked

- The workstation needs a persistent waveform display with a moving playhead.
- The display must communicate how the active rack is transforming the signal, not merely decorate playback.
- At minimum, users must be able to compare source and processed amplitude behavior while tuning.
- Changes in level, silence, dropouts, repeats, clipping, and other time-domain damage should be legible.
- Visualization must update during playback and remain synchronized with transport position.

## 7. Playback and audition — locked

- Playback controls are a first-class workstation surface, not a small utility row.
- Required controls: play/pause, stop/return, scrub, loop, loop range, current time, duration, and output level.
- Dry/processed A/B must be immediate and level-conscious.
- Users need a reliable way to audition a short region repeatedly while tuning a device.
- Playback state and playhead must remain stable while controls are adjusted.
- Export must match the audible rack path.

## 8. Sonic protection — locked

- Default settings and factory presets must avoid unexpectedly harsh, excessively loud, or overwhelmingly noisy states.
- A future randomizer must be musically constrained and sonically protected, not uniformly random.
- Continuous beds, tuning sweeps, feedback, wind, static, impulse events, and similar surprise-prone sources require conservative limits.
- The master output requires a dependable ceiling, limiting, soft clipping where appropriate, and visible clipping feedback.
- Opening the app must produce a dry, quiet, predictable state.
- Risky parameters should become progressively more sensitive near dangerous ranges rather than jumping linearly into them.
- Preset changes and randomization require smoothing or crossfading when audio is running.
- Undo/reset must always provide a quick escape from a bad state.

## 9. Presets and exploration — locked

- Presets should accelerate play without replacing manual control.
- Support both per-device presets and complete rack presets.
- Preset browsing should be auditionable during playback.
- Preset names should describe a device, condition, era, failure, environment, or recognizable recording behavior.
- The eventual randomizer should support scopes such as current device, active rack, subtle variation, or heavy damage.
- Randomization must respect safe parameter relationships and output limits.

## 10. Aesthetic direction — partially locked

Locked:

- Preserve the approved B&E visual language without turning the workstation into a marketing page.
- The interface should feel designed, authored, technical, strange, and playful.
- The rack itself is the visual hero.
- Avoid giant editorial headlines, explanatory journey cards, step rails, and large dead areas.
- Avoid generic SaaS panels and generic DAW imitation.

Proposed for review:

- B&E black/warm-paper foundation with restrained cyan and magenta signal accents.
- Hardware surfaces may introduce device-specific materials and colors: oxidized metal, painted steel, aged plastic, smoked acrylic, labels, tape, screws, vents, LEDs, and small displays.
- Wear should clarify character and history without reducing legibility.
- Typography should mix highly readable technical labels with a small amount of B&E transmission language.
- Motion should resemble meters, relays, signal traces, mechanical switches, scan lines, or energized circuitry.

## 11. Information architecture — locked

Persistent workstation frame:

- Top: robust transport, source name, time, loop range, A/B, playback state, and live signal feedback.
- Left: searchable/filterable device library with draggable hardware thumbnails.
- Center: horizontally flowing rack with cables, slots, or rails indicating order.
- In the top signal deck: synchronized source/processed waveform with playhead and loop controls.
- Bottom drawer, Inspector tab: a two-column focused-device editor with presets and deep controls. Clicking a rack module focuses and opens it without moving the rack.
- Bottom drawer, Mastering tab: post-chain level measurement, equalization, tonal noise cleanup, dynamics, saturation, ambience, limiting, clipping, and output format.
- Bottom drawer, Automation tab: exposed rack and mastering controls can be selected and drawn as point-and-curve lanes against the file waveform.
- Each control has one primary authoring location. Device wet mix and playable macros live on the hardware faceplate; the inspector does not repeat them; mastering does not duplicate device returns.

## 12. Interaction details — proposed

- Click-drag a knob vertically for broad adjustment; Shift-drag for fine adjustment.
- Double-click a parameter to reset it to its safe default.
- Mouse wheel adjustment is opt-in or requires focus so scrolling the page cannot accidentally change sound.
- Right-click or a visible menu provides reset, copy, paste, randomize safely, and remove.
- Dragging a device shows the exact insertion point and resulting signal order.
- Selected devices gain a clear outline and feed the focused inspector without rearranging the whole workspace.
- Device bypass remains available on the faceplate.
- A global panic/safe button immediately stops playback and returns output to a protected state.

## 13. Engineering constraints — proposed

- UI motion and waveform rendering must not run on the audio thread.
- Real-time-safe parameter changes should avoid rebuilding the entire graph.
- Graph rebuilds must be explicit, short, and pop-free when a device is inserted, removed, or structurally reconfigured.
- Visualization may be decimated, but it must remain temporally accurate and must not claim detail it does not measure.
- Input and output metering should expose peak state; loudness or gain matching should be added only when measured correctly.
- Desktop is the primary authoring surface. Narrow screens may use a scrollable rack or focused-device mode, but must not redefine the desktop product as a wizard.

## 14. Explicit anti-patterns — locked

Reject a design if it:

- asks users to describe their intention before they can hear or edit audio;
- hides the engines because they may look complicated;
- converts device emulation into a set of lifestyle/scenario choices;
- makes the rack secondary to instructions or branding;
- uses decorative waveform animation that is not derived from the signal;
- exposes random or preset states capable of sudden punishing noise;
- stops playback for ordinary parameter tuning;
- makes all devices look like interchangeable cards;
- removes precise values in pursuit of skeuomorphism;
- copies a conventional DAW so closely that LOST loses its own identity.

## 15. Acceptance test for the next prototype

The next visual prototype should not be considered successful until:

- audio can be loaded and played without answering anything;
- at least three devices can be dragged into a rack and reordered;
- the rack unmistakably reads left to right;
- each device looks and behaves like tactile hardware;
- manipulating a surface control gives immediate visible feedback;
- a real synchronized waveform or measured signal view responds to processing;
- transport supports scrubbing and a user-defined loop region;
- dry/processed A/B is obvious and output-conscious;
- presets never enable surprise-prone noise layers at unsafe levels;
- the whole experience feels like experimenting with a strange physical audio rig.

## 16. Decisions still needed from Azakaela

- Should the rack permit multiple instances of the same device?
- Should devices visually favor guitar pedals, 19-inch rack units, or a deliberate mixture chosen per engine?
- Should virtual patch cables be visible, implicit, or optional?
- Should the waveform default to overlaid source/output, split source/output, or a difference view?
- Should the main rack scroll horizontally as one continuous bench, or use fixed visible slots with paging?
- How literal should material wear and vintage hardware references become before they stop feeling like B&E?
