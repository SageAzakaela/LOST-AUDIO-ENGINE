# CD Engine VST3

CD Engine V2 is the native JUCE VST3/Standalone optical-disc failure processor. It uses the shared dependency-free CD core rather than the legacy plugin's mono processing loop.

## Signal model

- stereo-preserving optical transport: sector events are shared, while delay, history, interpolation, repeats, HF response, and program audio remain per channel;
- Hold, Mute, Interpolate, Repeat, and per-event Random concealment;
- radial scratches plus sine, triangle, square, saw, and random-pit damage geometry;
- decoder correction and short interpolation before terminal concealment;
- manual Damage and Skip triggers, recurring tracking loss, real per-channel history loops, and servo hunt;
- timing jitter, optical HF loss, servo mechanism sound, car-stereo multiband levelling, stereo link/width, wet/dry mix, input/output trim, soft clip, and a shared safety limiter;
- deterministic stochastic behavior and fixed 2.5 ms host latency.

Surface macros drive protected detailed parameters inside the audio processor, so the sound does not depend on the editor being open. Concealment mode and damage shape are always explicit: moving a macro can no longer switch Repeat back to Interpolate.

## Interface

The resizable Surface view presents a live optical deck, independent L/R input and output meters, read/correction/concealment/skip indicators, direct event triggers, sixteen reset-safe profiles, and the main physical controls. Advanced exposes the decoder, sector damage, tracking, transport, stereo output, and protection sections without scrolling.

## Build

From the repository root:

```powershell
cmake -S . -B build-cd -DLAE_BUILD_PLUGINS=ON -DBUILD_TESTING=OFF
cmake --build build-cd --config Release --target CDEngine_VST3 CDEngine_Standalone --parallel 2
```

Outputs:

- `build-cd/cd-vst3/CDEngine_artefacts/Release/VST3/CD Engine.vst3`
- `build-cd/cd-vst3/CDEngine_artefacts/Release/Standalone/CD Engine.exe`

Developer builds are intentionally unsigned. Release installers and signing remain deferred until the complete V2 plugin fleet is ready.
