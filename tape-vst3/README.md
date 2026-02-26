# Tape Engine VST3 (MVP)

First native port of `tape-engine` DSP.

## Build
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR="C:/SDKs/JUCE"
cmake --build build --config Release
```

Output VST3:
`build/TapeEngine_artefacts/Release/VST3/Tape Engine.vst3`

## Notes
- Core DSP ported: wow/flutter, AGC-style comp, saturation, dropouts, hiss/hum, limiter, tape tone filters.
- Compact custom UI styled consistently with the Transmission plugin family.
- Embedded tape deck SFX from `tape-engine/audio` (cassette + VHS banks).
- Includes factory presets, macro-linked controls, and parameter tooltips.
