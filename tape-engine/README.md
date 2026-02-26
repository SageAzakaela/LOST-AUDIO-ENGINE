# Tape Engine

Device simulator for cassette / VHS-style tape deck audio.

## Run locally

AudioWorklet requires serving over HTTP(S):

```powershell
python -m http.server 5173
```

Then open `http://localhost:5173/tape-engine/`.

## Host it (itch.io / GitHub Pages / any static host)

- You can host this as a static HTML5 project (no Python server needed for end users).
- The only requirement is HTTP(S) so AudioWorklet modules can load.
- Upload the whole `tape-engine/` folder contents, preserving paths.

## Tape Noise SFX

Tape Engine loads SFX from `tape-engine/audio/manifest.json`.
