# Web-reference parity harness

This directory establishes deterministic inputs and first-pass numerical comparisons for ports of the current web DSP. It does not claim that numerical similarity proves perceptual parity; level-matched listening remains a release gate.

## Generate diagnostic inputs

```powershell
python tests/parity/parity.py generate --out tests/parity/fixtures
```

The generator creates 44.1, 48, and 96 kHz PCM16 fixtures for silence, impulse response, logarithmic sweep, transient behavior, and stereo/channel handling. Real voice, music, ambience, and licensed device references must be added separately.

## Inspect or compare renders

```powershell
python tests/parity/parity.py analyze tests/parity/fixtures/impulse-48000.wav
python tests/parity/parity.py compare web-reference.wav native-port.wav
```

`compare` requires matching sample rate, channel count, sample width, and frame count. It reports maximum absolute sample error, RMSE, and normalized correlation per channel. Expected tolerances must be selected per engine and preset after the web reference renders are frozen; random processors must use the same documented seed.

## Golden-render layout

When reference capture begins, preserve files under a versioned external artifact bundle rather than casually replacing them in Git:

```text
web-reference-v1/
  manifest.json
  transmission/
    authentic/
    damaged/
    extreme/
  ...
```

Each manifest record should include source fixture hash, sample rate, engine/schema version, complete parameter state, preset metadata, random seed, web commit, browser/audio backend, whether the web master chain was enabled, output hash, and listening-review status.
