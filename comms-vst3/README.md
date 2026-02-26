# Comms Engine VST3

Native JUCE VST3/Standalone port of the web `comms-engine`.

## Included
- Channel models: Landline, Cell, Intercom, PA, Alarm
- Macro-to-advanced parameter mapping (bandwidth/drive/glitch/noise)
- Core comms DSP: AGC, saturation, sample-rate/bit reduction, packet loss, hum/hiss, optional alarm tone
- Parallel echo and room reverb sections
- Compact UI with presets and tooltip hints

## Build (Windows)
From `comms-vst3`:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR="C:/SDKs/JUCE"
cmake --build build --config Release
```

## Output
- `build/CommsEngine_artefacts/Release/VST3/Comms Engine.vst3`
- `build/CommsEngine_artefacts/Release/Standalone/Comms Engine.exe`
