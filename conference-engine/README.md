# Conference Engine

Browser-based VoIP / call-app simulator (Discord/Zoom/Skype/cell style).

Core behaviors:
- App-specific wideband, legacy, and cellular speech profiles
- Burst packet loss with speech-oriented PLC, mute, decay, or deep frame repeat
- Jitter-buffer slips and chunk duplication instead of chorus-style delay wobble
- Companded codec quantization, temporal resolution loss, and temporary bandwidth collapse
- Short repeated speech grains for recognizable robot/stutter failures
- Conference-style noise suppression, syllable-tail gating, AGC, DTX, and comfort noise

The primary controls describe the creative result. Advanced controls expose the
network and call-DSP failure mechanisms independently.

Preview and WAV export share the same processing chain.

