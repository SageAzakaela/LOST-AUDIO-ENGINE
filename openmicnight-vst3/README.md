# Open Mic Night VST3

JUCE-based VST3 + Standalone port of Open Mic Night.

## Build (Windows / VS 2022)

```powershell
& "C:\Program Files\CMake\bin\cmake.exe" -S "c:\Users\erina\Desktop\aaaaaaa\RADIO FILTER\openmicnight-vst3" -B "c:\Users\erina\Desktop\aaaaaaa\RADIO FILTER\openmicnight-vst3\build" -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR="C:/SDKs/JUCE"
& "C:\Program Files\CMake\bin\cmake.exe" --build "c:\Users\erina\Desktop\aaaaaaa\RADIO FILTER\openmicnight-vst3\build" --config Release
```

## Output

`openmicnight-vst3/build/OpenMicNight_artefacts/Release/VST3/Open Mic Night.vst3`

`COPY_PLUGIN_AFTER_BUILD` is disabled, so copy the bundle manually to your VST3 scan path.
