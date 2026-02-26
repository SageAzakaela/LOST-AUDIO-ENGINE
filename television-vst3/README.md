# Television Engine VST3 (MVP)

Compact CRT/TV coloration plugin, aligned with the suite UI style.

## Build
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR="C:/SDKs/JUCE"
cmake --build build --config Release
```

Output VST3:
`build/TelevisionEngine_artefacts/Release/VST3/Television Engine.vst3`

## Included
- TV speaker EQ contour + soft clipping + AGC/limiting
- Static/hiss/crackle + hum + CRT whine
- Embedded CRT bed sample from `television-engine/audio/crt.mp3`
- Compact custom UI with presets + tooltips
