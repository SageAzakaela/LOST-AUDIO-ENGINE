# Tape SFX manifest

`Tape Engine` can optionally mix in tape-noise SFX (processed through the same chain) during preview/export.

Current setup: Tape Engine loads banks from `tape-engine/audio/manifest.json` and expects the files to be in `tape-engine/audio/`.

Older/unused: `audio/tape-manifest.json` was an earlier manifest location.

Examples:

```json
{ "samples": ["cassette-hiss.mp3", "vhs-hum.wav"] }
```

or

```json
["cassette-hiss.mp3", "vhs-hum.wav"]
```
