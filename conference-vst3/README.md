# Conference Engine VST3

Native JUCE VST3/Standalone port of the web `conference-engine`.

## Included
- Conference modes: Discord, Zoom, Skype, Cell
- Macro controls mapped to advanced codec/jitter/packet params
- Core conference DSP: jitter delay, concealment modes, packet loss, gate, rate/bit reduction, robot artifacts, noise
- Compact UI with presets and tooltips

## Build (Windows)
From `conference-vst3`:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR="C:/SDKs/JUCE"
cmake --build build --config Release
```

## Output
- `build/ConferenceEngine_artefacts/Release/VST3/Conference Engine.vst3`
- `build/ConferenceEngine_artefacts/Release/Standalone/Conference Engine.exe`
