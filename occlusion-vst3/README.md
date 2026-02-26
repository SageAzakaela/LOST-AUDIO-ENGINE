# Occlusion Engine VST3

Native JUCE VST3/Standalone port of the web `occlusion-engine`.

## Included
- Occlusion tone path: dual HP/LP + dip/bump EQ
- Leak path for bypass-through material bleed
- Predelay + room reverb return path
- Material-aware macro mapping (drywall/brick/wood/curtain/door/glass)
- Compact UI with presets and tooltip hints

## Build (Windows)
From `occlusion-vst3`:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR="C:/SDKs/JUCE"
cmake --build build --config Release
```

## Output
- `build/OcclusionEngine_artefacts/Release/VST3/Occlusion Engine.vst3`
- `build/OcclusionEngine_artefacts/Release/Standalone/Occlusion Engine.exe`
