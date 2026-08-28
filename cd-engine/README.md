# CD Engine

Device simulator for CD/optical read artifacts: scratches/clicks, tracking skips/repeats, clock jitter, and error concealment.

Damage behavior and damage placement are separate controls:

- `Conceal Mode` chooses Hold, Mute, Interpolate, Repeat, or Random per failed read.
- `Damage Pattern` distributes defects as a radial scar, sine/triangle/square/saw sweep, or random pits around each disc revolution.
- `Trigger Damage` forces the current concealment behavior; `Trigger Skip` forces a read-head jump using real buffered program audio.

## Run locally

AudioWorklet requires serving over HTTP(S):

```powershell
python -m http.server 5173
```

Then open `http://localhost:5173/cd-engine/`.

## Notes

- Preview and export use the same DSP graph.
- Export uses `OfflineAudioContext` and writes mono 16-bit PCM WAV.

