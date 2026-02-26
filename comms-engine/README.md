# Comms Engine

Device simulator for telephone / cellphone / intercom / PA / alarm-style audio.

## Run locally

AudioWorklet requires serving over HTTP(S):

```powershell
python -m http.server 5173
```

Then open `http://localhost:5173/comms-engine/`.

## Host it (itch.io / GitHub Pages / any static host)

- You can host this as a static HTML5 project (no Python server needed for end users).
- The only requirement is that it’s served over HTTP(S) so AudioWorklet modules can load (hosting platforms already do this).
- Upload the whole `comms-engine/` folder contents, preserving paths.

## Notes

- Preview and export use the same DSP chain.
- Export uses `OfflineAudioContext` and writes mono 16-bit PCM WAV.
