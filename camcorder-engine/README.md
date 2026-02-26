# Camcorder Engine

Device simulator for consumer camera recording artifacts:

- Small mic muffling/coverage
- Auto gain control (AGC) pumping
- Handling thumps + cloth/rub noise
- Early-digital corruption (dropouts/holds/repeats + chirps)

## Run locally

AudioWorklet requires serving over HTTP(S):

```powershell
python -m http.server 5173
```

Then open `http://localhost:5173/camcorder-engine/`.

## Notes

- Preview and export use the same DSP graph.
- Export uses `OfflineAudioContext` and writes mono 16-bit PCM WAV.

## Wind SFX

If you have recorded wind SFX (steady wind bed + camera-hit gusts), put them in `camcorder-engine/audio/` and list them in `camcorder-engine/audio/manifest.json`:

```json
{
  "windBed": ["steady-wind.wav"],
  "windHits": ["camera-wind-hits.wav"]
}
```

