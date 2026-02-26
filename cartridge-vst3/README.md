# Cartridge Engine VST3

JUCE-based VST3 + Standalone port of the Cartridge Engine.

## Build (Windows / VS 2022)

```powershell
& "C:\Program Files\CMake\bin\cmake.exe" -S "c:\Users\erina\Desktop\aaaaaaa\RADIO FILTER\cartridge-vst3" -B "c:\Users\erina\Desktop\aaaaaaa\RADIO FILTER\cartridge-vst3\build" -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR="C:/SDKs/JUCE"
& "C:\Program Files\CMake\bin\cmake.exe" --build "c:\Users\erina\Desktop\aaaaaaa\RADIO FILTER\cartridge-vst3\build" --config Release
```

## Output

`cartridge-vst3/build/CartridgeEngine_artefacts/Release/VST3/Cartridge Engine.vst3`

`COPY_PLUGIN_AFTER_BUILD` is disabled, so copy the `.vst3` bundle manually to your DAW scan path.
