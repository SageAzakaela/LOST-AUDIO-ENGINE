# Camcorder Engine

Device simulator for consumer camera recording artifacts:

- Format-specific VHS-C, Video8/Hi8, MiniDV, digicam, and action-camera behavior
- Built-in electret, cheap mono, stereo camera, waterproof, and shotgun microphone responses
- Small mic muffling/coverage
- Auto gain control (AGC), recovery timing, and noise-floor pumping
- Analog transport flutter versus digital converter/block corruption
- Handling thumps + cloth/rub noise
- Format-aware dropouts/holds/repeats, analog tracking buzz, and digital chirps
- Filtered wind-pressure overload and camera motor bleed

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

