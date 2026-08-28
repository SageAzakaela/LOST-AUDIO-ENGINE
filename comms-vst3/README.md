# Comms Engine V2

Native JUCE VST3/Standalone communications-hardware emulator. V2 runs the
dependency-free `native/core` Comms processor rather than the legacy
editor-driven parameter mapping.

## Included

- Distinct Landline, Cellular, Intercom, PA, and Alarm Panel signal paths
- Carbon granules, companded landline coding, predictive cellular coding,
  half-duplex gating, packet loss, converter reduction, line hum, and hiss
- Signal-excited receiver, enclosure, and horn resonances
- Distance, early reflection, and compact-room processing
- Protected Surface macros plus direct Advanced hardware controls
- Resizable no-scroll console UI, live voice display, metering, and 16 profiles
- Deterministic stochastic behavior and portable native safety tests

## Developer build (Windows)

From the repository root with the pinned JUCE configuration already generated:

```powershell
cmake --build build-tape-dev --config Release --target CommsEngine_VST3 CommsEngine_Standalone --parallel 2
```

Outputs:

- `build-tape-dev/comms-vst3/CommsEngine_artefacts/Release/VST3/Comms Engine.vst3`
- `build-tape-dev/comms-vst3/CommsEngine_artefacts/Release/Standalone/Comms Engine.exe`

These are unsigned development builds. Release installers and signing remain
deferred until the individual V2 fleet is complete.
