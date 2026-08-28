# Architecture and signal flow

## Authority boundaries

Lost Audio Engine currently has two implementations:

1. **Web reference:** the active product, current UX authority, and sonic reference.
2. **JUCE VST3 projects:** earlier native ports that preserve plugin identities and useful implementation work but do not yet match the web graph feature-for-feature or preset-for-preset.

When behavior disagrees, do not silently tune the web version toward an older plugin. Capture the web state, compare level-matched renders, and make an explicit parity decision.

## Web entry points

- `index.html` is the canonical workstation page.
- `lame/styles.css` contains the current workstation presentation.
- `transmission/index.html` and root `styles.css` preserve the legacy standalone Transmission interface.
- `lame/index.html` redirects older `/lame/` bookmarks to the root workstation.
- `lame/src/main.js` owns application state, transport, device shelf/rack interaction, inspector rendering, presets, automation, visualization, access-mode integration, and export orchestration.
- `lame/src/audio/graph.js` assembles device graphs and the final mastering lane.
- `lame/src/audio/master-noise-reducer-processor.js` is the mastering noise-reduction AudioWorklet.

The historical directory name `lame/` means “Lost Audio Media Engine.” It is an internal path, not a second product name.

## Signal flow

```text
decoded file
  → source trim
  → ordered rack modules (per-device bypass + wet/dry)
  → master high/low cuts + 10-band EQ
  → adaptive noise reducer
  → bus compression
  → parallel saturation / delay / reverb color stage
  → limiter + soft clipper + ceiling
  → monitor gain
  → speakers and waveform/meter feedback

same source + rack + master state
  → OfflineAudioContext
  → PCM16 WAV export
```

Monitor volume is audition-only. Source trim, rack processing, mastering, automation, mono output, and output protection are part of the rendered result.

## Engine topology

Each rack engine owns its browser graph and presets:

| Rack type | Graph source | Presets |
| --- | --- | --- |
| Transmission | `src/audio/graph.js` | `src/presets.js` |
| Comms | `comms-engine/src/audio/graph.js` | `comms-engine/src/presets.js` |
| Conference | `conference-engine/src/audio/graph.js` | `conference-engine/src/presets.js` |
| Tape | `tape-engine/src/audio/graph.js` | `tape-engine/src/presets.js` |
| Television | `television-engine/src/audio/graph.js` | `television-engine/src/presets.js` |
| Cartridge | `cartridge-engine/src/audio/graph.js` | `cartridge-engine/src/presets.js` |
| CD | `cd-engine/src/audio/graph.js` | `cd-engine/src/presets.js` |
| Camcorder | `camcorder-engine/src/audio/graph.js` | `camcorder-engine/src/presets.js` |
| Obfuscation | `occlusion-engine/src/audio/graph.js` | `occlusion-engine/src/presets.js` |

Graph factories return a stable boundary used by the rack: input, output, settings application, deterministic reset where stochastic DSP exists, and any explicit cleanup/event hooks required by the model.

## Realtime and offline parity

Realtime audition and WAV export intentionally share graph factories. Any feature added only to UI-side playback code will disappear from export; any feature added only to offline rendering will misrepresent auditioning. A change is incomplete until both paths use it or the difference is explicitly documented.

Random processors receive controlled seeds for diagnostic rendering. Product playback may vary naturally, but test and parity captures must record the seed.

## Presets and state

- Per-engine factory presets live beside each engine.
- Protected rack/master presets are assembled in `lame/src/presets.js`.
- User engine and rack presets are stored in local browser storage.
- Automation lanes target exposed module and mastering parameters by stable identifiers.
- Parameter changes must propagate to the graph, inspector, module surface, automation target, saved preset, and export state.

This propagation requirement exists because a visually moving control that does not affect DSP is a release-blocking failure.

## Audio assets

Audio beds and mechanical events are discovered through `manifest.json` files where supported. Preserve relative paths and record provenance before adding new audio. Do not add reference recordings merely because they are technically downloadable.

## Hosted integration boundary

The source workstation is a static application and does not require an account server. On `bande.digital` only, `lame/src/main.js` enables the hosted fifteen-minute/email access integration at `/api/color-systems/lost-access`. Localhost, itch.io, GitHub Pages, and independent static hosts remain unlimited.

The gate does not upload the user’s audio. It only records the submitted email and issues the hosted unlock state.

## Native boundary

Each `*-vst3/` directory is currently an independent JUCE target with its own processor, editor, parameter state, presets, and duplicated DSP. The intended V2 architecture is a portable shared DSP core with adapters for WebAssembly/AudioWorklet and JUCE. That migration begins only after the web reference and platform matrix are frozen.
