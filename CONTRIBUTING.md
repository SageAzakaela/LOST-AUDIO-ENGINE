# Contributing to Lost Audio Engine

Bug reports, reproducible listening observations, platform results, device references, and focused documentation corrections are welcome through [GitHub Issues](https://github.com/SageAzakaela/LOST-AUDIO-ENGINE/issues).

## Before opening an issue

- Search existing issues and the [feedback tracker](docs/feedback-tracker.md).
- Identify the web workstation, engine, VST3, platform, preset, or release package involved.
- Include exact reproduction steps, operating system, browser/DAW, sample rate, buffer size, mono/stereo state, input format, and version/commit where relevant.
- Use a short non-confidential audio example only when you have permission to share it.
- Separate what you heard from what you infer caused it.

## Quality expectations

A DSP change should include evidence that:

- exposed controls affect the intended graph state;
- realtime audition and offline export agree;
- deterministic processors remain deterministic under a recorded seed;
- stochastic/noise/event branches stay bounded;
- presets are useful at safe defaults;
- subtle and authentic settings remain available alongside extreme ones;
- state, automation, and migration implications are understood;
- the result has been listened to, not only graphed.

UI changes must preserve the selected-device rack workflow, responsive laptop legibility, keyboard/focus behavior, robust transport, and deliberate mobile fallback defined in [design constraints](docs/design-constraints.md).

## Validation

Run the relevant checks in [docs/testing.md](docs/testing.md). At minimum:

```powershell
node scripts/validate-repository.mjs
node tests/obfuscation-body-harness.mjs
```

Describe what was automated, what was manually listened to, and what remains unverified.

## Audio assets

Do not add copyrighted, proprietary, client, or personal recordings without explicit distribution rights. New runtime audio must include source/provenance, creator, license or permission, modifications, and attribution requirements. Reference material used only for listening research should remain outside the repository.

## Pull-request boundary

The repository currently has no published source license. Please open an issue before preparing a substantial code contribution so scope, ownership, licensing, and the correct reference behavior can be agreed first. Public visibility alone does not grant reuse rights.
