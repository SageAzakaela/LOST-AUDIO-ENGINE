# Release workflow

## Principles

- Source control contains source, documentation, manifests, and small required runtime assets.
- Generated web ZIPs, VST3 bundles, installers, and preserved published artifacts do not live at the repository root.
- Public binaries belong on itch.io or GitHub Releases with hashes and platform notes.
- A release is identified by a commit SHA and a reproducible artifact hash.

## Web package

From a validated checkout:

```powershell
node scripts/validate-repository.mjs
node tests/obfuscation-body-harness.mjs
python make-itch-zip.py
```

The packager writes `dist/LostAudioEngine-web-itchio.zip` and preserves forward-slash archive paths. Inspect the ZIP before upload:

```powershell
python -m zipfile -l dist/LostAudioEngine-web-itchio.zip
Get-FileHash dist/LostAudioEngine-web-itchio.zip -Algorithm SHA256
```

On Linux, use `sha256sum` for the same hash.

The package is the standalone web edition. The B&E Digital deployment also needs its site navigation and hosted access API; those server-side pieces are not part of this static ZIP.

## Native packages

Native releases must come from clean builds and include:

- product and semantic version;
- commit SHA;
- operating system and CPU architecture;
- plugin format and bundle identity;
- compiler, CMake, and exact JUCE commit;
- validation and DAW matrix;
- preset/state migration notes;
- SHA-256 for every downloadable artifact;
- known limitations.

Do not replace a published artifact under the same filename/version without preserving and explaining the previous hash.

## Release checklist

1. Confirm the intended commit and clean/known working-tree state.
2. Run repository, DSP, browser, responsive, and listening gates appropriate to the release.
3. Generate artifacts into ignored `dist/` or another isolated staging directory.
4. Inspect archive contents and reject build directories, credentials, private audio, and unrelated files.
5. Verify the artifact on a clean machine or account.
6. Record hashes, platform data, and known limitations.
7. Tag the source commit and publish the immutable artifacts.
8. Smoke-test the public download and installation/launch path.
9. Preserve release evidence in notes; update [release-audit.md](release-audit.md) only when historical identity evidence changes.
