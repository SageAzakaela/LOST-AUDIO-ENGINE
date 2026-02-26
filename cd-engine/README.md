# CD Engine

Device simulator for CD/optical read artifacts: scratches/clicks, tracking skips/repeats, clock jitter, and error concealment.

## Run locally

AudioWorklet requires serving over HTTP(S):

```powershell
python -m http.server 5173
```

Then open `http://localhost:5173/cd-engine/`.

## Notes

- Preview and export use the same DSP graph.
- Export uses `OfflineAudioContext` and writes mono 16-bit PCM WAV.

