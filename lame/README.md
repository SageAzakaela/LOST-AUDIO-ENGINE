# Lost Audio Engine

(Internally: `lame/` — Lost Audio Media Engine.)

Master suite that chains the individual engines as modular wet/dry units.

## Run locally

AudioWorklet requires serving over HTTP(S):

```powershell
python -m http.server 5173
```

Then open `http://localhost:5173/`. The legacy `/lame/` URL redirects to the current workstation so old bookmarks do not expose the retired interface.

## Getting started and device support

- `Guide ?` opens the optional field manual and seven-step workstation walkthrough.
- The guide never blocks loading or auditioning audio.
- The complete rack is designed for laptop and desktop browsers at least 1024 px wide.
- Phones and narrow touch devices receive a deliberate desktop-only handoff instead of a cramped or partially functioning rack.
- Audio remains local to the browser.

## Notes

- Preview and export use the same modular DSP graph.
- Export uses `OfflineAudioContext` and writes 16-bit PCM WAV (mono or stereo depending on the `Mono Output` toggle).
