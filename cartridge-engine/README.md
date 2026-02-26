# Cartridge Engine

Browser-based retro cartridge / console audio degrader.

## Run locally

AudioWorklet requires serving over HTTP(S):

```powershell
python -m http.server 5173
```

Then open `http://localhost:5173/cartridge-engine/`.

## Notes

- Preview and export use the same DSP chain.
- Export uses `OfflineAudioContext` and writes mono 16-bit PCM WAV.

