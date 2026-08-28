# Platform and device integration

Status: next implementation phase after repository preservation and documentation.

The immediate goal is to make the web reference dependable beyond the current Windows development environment before rebuilding the VST3 fleet. This phase is about verified compatibility, not merely opening the page on another machine.

## Ordered targets

1. Linux desktop on x86_64 using current Chromium/Chrome.
2. Linux desktop on Firefox.
3. Linux ARM64 where suitable hardware is available.
4. macOS on Apple Silicon and Intel where available.
5. Broader Windows browser/audio-device coverage.
6. Deliberate evaluation of tablets and touch devices; the current product remains desktop-only until the rack can be presented without damaging its workflow.

## Linux audio environments

Browser Web Audio normally reaches hardware through the browser and operating-system audio stack. Test at least:

- PipeWire;
- PulseAudio compatibility where still present;
- common built-in outputs;
- USB audio interfaces;
- Bluetooth output as a latency/stability edge case, not a reference listening path;
- default-device changes before and during a session;
- 44.1, 48, and 96 kHz interface configurations.

Direct ALSA, JACK, and PipeWire-native integration is not currently implemented. If a future desktop wrapper or live-input mode requires direct device control, design that as a separate adapter rather than mixing platform APIs into device DSP.

## Web compatibility gates

For each browser/OS/architecture combination, verify:

- root workstation boot and desktop/mobile boundary;
- WAV and MP3 decoding from the file picker and drag/drop;
- AudioWorklet module loading for every engine;
- realtime start, pause, seek, loop, source/processed comparison, and repeated file replacement;
- module add, reorder, remove, bypass, wet/dry, inspector, presets, and random/event controls;
- automation playback and export;
- master EQ, noise learning/reduction, color/space, dynamics, meters, and safety ceiling;
- deterministic offline WAV export and a playable downloaded file;
- page responsiveness during long files, SFX beds, rapid parameter movement, and repeated exports;
- local-source unlimited mode and the separate B&E hosted access flow;
- no horizontal overflow at supported laptop dimensions;
- clear fallback on unsupported/touch/mobile configurations.

## Device and session matrix

Capture exact evidence rather than a single “Linux works” checkbox:

| Area | Minimum evidence |
| --- | --- |
| CPU | x86_64; ARM64 when hardware is available |
| Browser | name, exact version, Web Audio backend if reported |
| Audio | device/interface, output path, configured sample rate |
| Input files | PCM WAV variants, stereo/mono, MP3, short/long files |
| Output | exported WAV hash, channels, sample rate, peak, duration |
| Stress | repeated play/stop, device reorder, bed loading, export, file replacement |
| Visual | supported laptop sizes and deliberate mobile boundary |

## Completion gate

Platform integration is complete enough to begin VST3 V2 work when:

- the automated repository and browser DSP harnesses pass on the supported matrix;
- a representative listening set has been reviewed on Windows and Linux;
- browser/platform exceptions are documented with honest fallbacks;
- export files are verified independently of the browser;
- long-session and repeated-load failures are resolved or have reproducible tracked issues;
- the supported-platform table can distinguish verified, experimental, unsupported, and untested states.

Do not promise Linux packages, live device capture, mobile support, or a desktop wrapper until the chosen delivery model has been implemented and tested.
