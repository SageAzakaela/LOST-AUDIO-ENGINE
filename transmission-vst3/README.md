# Transmission Engine VST3 (V2 developer build)

This is the active native V2 port of the web Transmission Engine.

## What this build targets
- `VST3`: load in Ableton Live 11/12 on Windows.
- `Standalone`: useful for quick DSP checks.
- Audacity can host VST3 effects in modern versions.

## Current scope

- dependency-free shared DSP core with deterministic tests;
- causal carrier drift using time displacement instead of tremolo;
- level-matched transmitter saturation, compression, receiver filtering,
  dropout, interference, protected RF noise, and 1–6 generations;
- true input, latency-aligned Mix, output, metering, and fixed host latency;
- signal-driven squelch clicks or dispatch tones;
- tuning/search mode with embedded assets (`dispatch.mp3`, `tuning1..5.mp3`);
- resizable Surface/Advanced receiver interface with no scrolling;
- sixteen reset-safe profiles spanning clean, authentic, damaged, and
  deliberately exaggerated signals.

## Prerequisites
- CMake 3.22+
- Visual Studio 2022 with C++ workload
- JUCE source checkout

## Build (Windows)
From this folder (`transmission-vst3`):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR="C:/path/to/JUCE"
cmake --build build --config Release
```

The VST3 is generated under the build output and copied to your system VST3 folder when supported by JUCE settings.

## Notes

This is an unsigned developer build, not a public release. Numerical tests and
successful compilation do not replace level-matched listening, DAW scanning,
automation, state-recall, and UI approval.
