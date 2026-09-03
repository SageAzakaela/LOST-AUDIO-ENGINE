# Lost Audio Suite VST3

Lost Audio Suite is the all-in-one serial rack for the ten Lost Audio Engine
device and space models. It currently provides six repeatable slots, drag and
button reordering, independent slot state, two global modulation macros, twelve
factory chains, fixed latency compensation, and a hard master safety path.
Television slots retain the individual plug-in's embedded CRT bed.

Open Mic Night feedback is disarmed by every chain preset and protected by an
off-before-on eligibility latch after insertion or state recall.

## Developer build

From the repository root:

```powershell
cmake -S . -B build -DLAE_BUILD_PLUGINS=ON -DBUILD_TESTING=OFF
cmake --build build --config Release --target LostAudioSuite_VST3 --parallel 2
```

The unsigned bundle is generated at:

```text
build/suite-vst3/LostAudioSuite_artefacts/Release/VST3/Lost Audio Suite.vst3
```

`COPY_PLUGIN_AFTER_BUILD` is disabled. Public installers remain deferred until
the Suite and individual fleet pass hands-on listening, DAW, state, automation,
UI, platform, and release-signing gates.
