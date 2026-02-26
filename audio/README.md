# /audio tuning samples

Put short tuning/static recordings here (WAV/MP3). Then list them in `audio/manifest.json` so the app can show them in the **Tuning Source** dropdown.

Example:

```json
{
  "samples": [
    { "id": "tune1", "name": "Dial Sweep 1", "file": "dial-sweep-1.wav" },
    { "id": "tape", "name": "Tape Static", "file": "tape-static.mp3" }
  ]
}
```

Also supported:

```json
{ "samples": ["dial-sweep-1.wav", "tape-static.mp3"] }
```
