# Transmission Engine VST3 (MVP)

This folder contains a first native plugin port of the web `Transmission Engine` DSP.

## What this build targets
- `VST3`: load in Ableton Live 11/12 on Windows.
- `Standalone`: useful for quick DSP checks.
- Audacity can host VST3 effects in modern versions.

## Current scope
- Core radio tone shaping: dual HP/LP, box dip, mid peak.
- Saturation/compression path.
- Lo-fi degradation: crush, wow, dropout, crackle, noise color/hiss.
- Tuning mode with embedded real assets (`dispatch.mp3`, `tuning1..5.mp3`) plus `Random Tuning`.
- Compact custom GUI styled like a 90s digital radio rack.
- Factory presets and hover hints for control behavior.
- Multi-pass stage stacking (`passes` 1..6).

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
- This is an MVP port, not yet sample-for-sample identical to the web worklet graph.
- Current build focuses on radio/tuning coloration and compact workflow.
