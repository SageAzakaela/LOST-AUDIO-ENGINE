# CD Engine VST3

Native JUCE VST3/Standalone port of the web `cd-engine` effect.

## Included
- CD concealment modes: Hold, Mute, Interp, Repeat
- Error bursts, repeat-frame behavior, scratch click transients
- Jitter delay modulation and servo noise layer
- HF loss voicing, soft clip, ceiling limiter, output gain
- Car-style leveling (`carComp`)
- Compact UI with presets + parameter tooltips

## Build (Windows)
From `cd-vst3`:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR="C:/SDKs/JUCE"
cmake --build build --config Release
```

## Output
- `build/CDEngine_artefacts/Release/VST3/CD Engine.vst3`
- `build/CDEngine_artefacts/Release/Standalone/CD Engine.exe`
