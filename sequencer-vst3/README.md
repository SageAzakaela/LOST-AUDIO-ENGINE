# Lost Audio Sequencer

Lost Audio Sequencer is B&E Digital's 16-step, host-synced device-damage VST3.
Each step runs one of the ten shared Lost Audio engines and stores its own
Character, Damage, Probability, Mix, and Device Model controls.

## Workflow

1. Choose a musical division and pattern length.
2. Click a pad to inspect it; double-click to arm or rest it.
3. Choose the device and tune its five focused controls.
4. Press the DAW transport. Use Audition only when working without a host clock.
5. Start from a factory pattern or use Safe Random for bounded variations.

The output safety stage is enabled by default. Sequenced Open Mic steps cannot
arm feedback, and Safe Random deliberately limits density and intensity.

## Build and test

```powershell
cmake -S . -B build-tape-dev -DLAE_BUILD_PLUGINS=ON -DBUILD_TESTING=ON
cmake --build build-tape-dev --config Release --target LostAudioSequencer_VST3 LostAudioSequencer_Standalone lost_audio_sequencer_state_tests lost_audio_sequencer_clock_tests --parallel 2
ctest --test-dir build-tape-dev -C Release -R "lost_audio_(sequencer_clock|sequencer_state|suite_core)_tests" --output-on-failure
```

Audible approval in a DAW remains required before release.
