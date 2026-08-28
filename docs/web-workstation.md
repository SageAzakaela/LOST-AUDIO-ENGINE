# Web workstation guide

## Requirements

- A modern desktop browser with Web Audio, AudioWorklet, OfflineAudioContext, Canvas, and ES module support.
- A viewport at least 1024 pixels wide.
- HTTP or HTTPS. `file://` cannot load the AudioWorklet graph correctly.

Chrome and Edge on Windows are the current development baseline. Linux, Firefox, Safari, ARM64, and broader hardware/device combinations belong to the next formal compatibility phase; do not infer support from a page merely loading.

## Start locally

From the repository root:

```powershell
python -m http.server 5173
```

Open `http://localhost:5173/`. No build step or dependency installation is required.

## Core workflow

1. Load a WAV or MP3 file.
2. Use source trim to establish sane headroom.
3. Drag devices from the shelf into the left-to-right rack.
4. Adjust the important controls on the hardware surface.
5. Select a device to open its full inspector.
6. Use Mastering for measured cleanup, space, dynamics, and protected output.
7. Use Automation to draw file-time changes for exposed controls.
8. Compare source and processed playback, then export WAV.

The optional Guide explains the workstation without blocking audio loading or playback.

## Gain staging

- **Source trim** changes the signal entering the rack and therefore affects processing and export.
- **Device mix** controls each module’s wet/dry contribution.
- **Master gain and dynamics** affect the rendered file.
- **Monitor volume** affects listening only and is not printed into the export.

When a result becomes harsh or noisy, first reduce device damage/noise and confirm source headroom. Use mastering noise reduction for unwanted constant or low-level contamination; it is not a substitute for correcting an unstable device state.

## Presets

Factory presets are protected starting points. Rack presets build complete diegetic scenarios such as next-room television, answering machines, archived surveillance, damaged game cutscenes, scratched-disc calls, and pirate broadcasts.

Presets must be useful at their default input level, output-safe, and clearly identify whether they target subtle, authentic, damaged, or exaggerated behavior.

## Automation

Automation operates against file time rather than a DAW transport. Expose a module or mastering control, choose it in the Automation drawer, and draw points against the waveform. Playback and offline export read the same lane data.

## Export

Export uses OfflineAudioContext and writes PCM16 WAV. Mono output is enabled by default in the current master state. The LUFS-M display is a tuning estimate, not a standards-compliant delivery measurement; scan final deliverables with a dedicated loudness tool when a specification matters.

## Privacy and hosted access

Audio decoding and processing occur locally in the browser. The B&E Digital edition enables an email unlock after fifteen minutes of active use; the email request goes to B&E’s hosted endpoint, while audio remains local. Source builds and other hosts do not enable that gate.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Workstation never appears | Confirm desktop width and that JavaScript is enabled |
| AudioWorklet error | Serve through HTTP(S), then check the browser console for a missing relative module |
| Playback is silent | Confirm a file is decoded, monitor volume is up, and the active output is not muted |
| Sudden harsh output | Reduce source trim, device damage/noise, master makeup, and wet mix; check the ceiling |
| A control moves but sound does not | Reproduce in the parameter-influence harness and file a bug; this is not cosmetic |
| Preview and export differ | Record the preset, complete state, automation, browser, sample rate, and exact reproduction steps |
| B&E unlock fails locally | Local source should show unlimited; hard-refresh to ensure the current `main.js` is loaded |
