# Open Mic Night VST3

Open Mic Night is Lost Audio Engine's live-scene processor: microphone,
preamp, monitor, PA, venue, audience, and deliberately armed feedback in one
stereo physical model. V2 uses the dependency-free processor in `native/core`
and keeps the original `OmNt` plug-in identity and V1 parameter prefix.

Feedback is disarmed in every factory preset. Arm it explicitly, and keep the
Safety control engaged while auditioning. The feedback loop is internally
bounded and followed by a separate output ceiling, but normal monitoring
precautions still apply.

The V2 developer-QA bundle embeds four selectable audience ambiences plus
separate applause and cheer reactions from the web tool. Reactive mode listens
to performance energy and responds after phrase endings; Steady Ambience and
Manual / Clocked are explicit alternatives. Beds are loudness-normalized and
crossfaded when switched. Public distribution of those recordings still
requires explicit provenance and rights confirmation. Additional legacy MP3
beds remain excluded until their license and provenance metadata are complete.

## Developer build

From the repository root:

```powershell
cmake -S . -B build -DLAE_BUILD_PLUGINS=ON -DBUILD_TESTING=OFF
cmake --build build --config Release --target OpenMicNight_VST3 --parallel 2
```

The unsigned developer bundle is generated at:

```text
build/openmicnight-vst3/OpenMicNight_artefacts/Release/VST3/Open Mic Night.vst3
```

`COPY_PLUGIN_AFTER_BUILD` is disabled. Copying into a DAW scan path remains an
explicit local QA step; public installers stay deferred until the fleet is
approved and the release-signing pipeline exists.
