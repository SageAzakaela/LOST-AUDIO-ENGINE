# Camcorder Engine VST3

JUCE-based VST3 + Standalone port of the Camcorder Engine.

## Build (Windows / Visual Studio 2022)

```powershell
& "C:\Program Files\CMake\bin\cmake.exe" -S "c:\Users\erina\Desktop\aaaaaaa\RADIO FILTER\camcorder-vst3" -B "c:\Users\erina\Desktop\aaaaaaa\RADIO FILTER\camcorder-vst3\build" -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR="C:/SDKs/JUCE"
& "C:\Program Files\CMake\bin\cmake.exe" --build "c:\Users\erina\Desktop\aaaaaaa\RADIO FILTER\camcorder-vst3\build" --config Release
```

## Output

Built plugin bundle:

`camcorder-vst3/build/CamcorderEngine_artefacts/Release/VST3/Camcorder Engine.vst3`

Because `COPY_PLUGIN_AFTER_BUILD` is disabled, copy the `.vst3` bundle manually into your VST3 scan folder (for example `%CommonProgramFiles%\VST3`) or add the build output directory in your DAW plugin paths.
