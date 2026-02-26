# Lost Audio Engine

(Internally: `lame/` — Lost Audio Media Engine.)

Master suite that chains the individual engines as modular wet/dry units.

## Run locally

AudioWorklet requires serving over HTTP(S):

```powershell
python -m http.server 5173
```

Then open `http://localhost:5173/lame/`.

## Notes

- Preview and export use the same modular DSP graph.
- Export uses `OfflineAudioContext` and writes 16-bit PCM WAV (mono or stereo depending on the `Mono Output` toggle).
