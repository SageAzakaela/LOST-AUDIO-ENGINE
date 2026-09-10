# Mac plugin beta

The Mac build targets macOS 11 or newer and includes native `arm64` and `x86_64`
code in every bundle. All thirteen products build as VST3 and Audio Units v2
(`.component`). `AU_SANDBOX_SAFE` permits use in sandboxed AU hosts; existing
manufacturer, plugin and bundle identifiers remain unchanged.

## Compatibility scope

| Host/system | Included format | Evidence required |
| --- | --- | --- |
| Apple silicon Mac | native arm64 AU / VST3 | Mac arm64 CI validation |
| Intel Mac | native x86_64 AU / VST3 | Mac Intel CI validation |
| Logic Pro / GarageBand | AU | AU validation plus actual DAW session |
| DAW with VST3 support | VST3 | pluginval plus actual DAW session |
| Pro Tools | AAX is not included | Separate AAX implementation/distribution |
| macOS before 11 | Not targeted | No compatibility claim |

The minimum deployment target is a build constraint, not evidence of testing
every intervening macOS release. CI uses macOS 15 on both CPU families. Passing
validators does not prove compatibility with every DAW version or complete
sonic parity with the browser edition.

References: [Logic Audio Units](https://support.apple.com/guide/logicpro/lgcp22a0dab0/mac),
[Avid plugin formats](https://resources.avid.com/SupportFiles/PT/Audio_and_MIDI_Plugins_Guide_2024.10.pdf),
[GitHub runner architectures](https://docs.github.com/en/actions/reference/runners/github-hosted-runners),
[pluginval](https://github.com/Tracktion/pluginval).

## Reproduce the build

With Xcode command-line tools, CMake 3.22+ and Ninja on a Mac:

```sh
cmake -S . -B build-mac -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 '-DLAE_PLUGIN_FORMATS=VST3;AU'
cmake --build build-mac --parallel 3
python3 scripts/macos_plugins.py stage --build build-mac --output bundles
```

JUCE remains pinned at the revision in the root CMake file. AU is added only on
Mac by default; Windows/Linux keep VST3 and Standalone. Individual plugin
directories share the same defaults. `LAE_PLUGIN_PROJECTS` can select a
semicolon-separated subset of project directories for development builds.
Explicit architecture/format/deployment overrides are respected, but the Mac
distribution checks require both architectures and a deployment target no newer
than 11.0. Nothing is automatically copied into a user's plugin scan folders.

## Automated gates

`.github/workflows/macos-plugins.yml` builds four groups and tests the exact
same universal bundles on native Intel and Apple silicon runners. It checks:

- 14 portable DSP test executables on each CPU;
- both Mach-O architecture slices and minimum OS load commands;
- system-only runtime library dependencies, intact signatures and identities;
- Apple `auval` for every Audio Unit;
- pinned pluginval 1.0.4, strictness 5, for both formats, including editor tests;
- 44.1/48/96 kHz and 64/128/256/512/1024-sample pluginval coverage;
- per-binary hash agreement between the validated bundles and packaged files.

Validation logs and exact source commits are retained as CI artifacts. The final
developer ZIP is emitted only after all required jobs pass. Build-shard artifacts
alone are compilation evidence, not a validated handoff.

`.github/workflows/macos-validation.yml` can revalidate existing build shards
without recompiling. It requires all four original build jobs and both native
DSP jobs to have passed. Reports retain the original binary source SHA separately
from the validation-tool SHA and verify the original binary hashes. In disposable
CI runners, the components are installed to the system folder and the AU registrar
is refreshed once before validation to model a fresh login. The recipient's
installer does not include a registrar-reset script.

`.github/workflows/macos-installer.yml` accepts a successful build run ID and
creates a standard Installer `.pkg` from its exact validated ZIP. It checks the
source SHA and every plugin hash before packaging. The installer is then run on
fresh Intel and Apple silicon runners, with all installed binary hashes checked
and every installed AU scanned again. Only after those jobs pass is the
`be-digital-mac-beta-installer` artifact emitted.

The installer uses standard system plugin directories, disables bundle relocation,
contains no custom install scripts, and requires normal installer authorization.
It replaces existing same-name system installations. User-folder duplicates and
project backups are covered in [installation instructions](../installer/macos/INSTALL.txt).

## Private beta trust and distribution

The current path uses ad-hoc plugin signatures and an unsigned installer. It is
not Apple notarized. A recipient may need to approve this specific installer
through macOS Privacy & Security after confirming its origin. Managed Macs may
disallow this. No global Gatekeeper changes or quarantine-clearing scripts are
part of the package. See [Apple's approval instructions](https://support.apple.com/102445).

For the smoother public release path, sign all plugin bundles with Developer ID
Application, sign the installer with Developer ID Installer, submit it using
`notarytool`, require an Accepted result, staple the installer and verify its
Gatekeeper assessment. Keep certificates and notarization credentials outside
source control. See [Apple's Developer ID guidance](https://developer.apple.com/developer-id/).

Before calling a recipient's setup verified, record their DAW/version and macOS
version, then check scan, insertion, audio, UI resizing, factory presets,
automation and save/close/reopen. Test on a copy of a project.

## Verified private beta, September 9, 2026

- Binary source: `3bf921df4f045f5445d5e5ce7a281071880f48f8`.
- [Build and portable DSP evidence](https://github.com/SageAzakaela/LOST-AUDIO-ENGINE/actions/runs/34432553049):
  all thirteen universal AU/VST3 builds passed; 14 portable DSP tests passed on
  each CPU. This first run's AU discovery tests failed before the CI registration
  fix; use the subsequent passing validation evidence below.
- [Passing plugin validation](https://github.com/SageAzakaela/LOST-AUDIO-ENGINE/actions/runs/34433772109):
  52 pluginval executions and 26 Apple AU validations passed across native arm64
  and x86_64, on macOS 15.7.9. The original binary hashes are unchanged.
- [Passing final installer validation](https://github.com/SageAzakaela/LOST-AUDIO-ENGINE/actions/runs/34434651331):
  installation succeeded on both CPUs, all 26 installed bundle hashes matched,
  and all thirteen installed AUs passed again on each CPU. Neither final installer
  test needed a registrar refresh. Logout/login is only a recipient troubleshooting
  fallback, not a required normal installation step.
- `BE-Digital-Mac-Beta-3bf921df.pkg` SHA-256:
  `301a3621d40e66e314c1a8bf45d6036571274f92773b22c608a8b8e2979925f9`.
- The local handoff ZIP `dist/macos/BE-Digital-Mac-Beta-3bf921df.zip` contains the
  installer, instructions, manifest and installer checksum. Its SHA-256 is
  `c271ea8410c649b28c066222451ef320500d3bb2222c73adb466b7894832741f`.
  The downloaded installer and the installer inside the ZIP both matched the
  installed/tested package hash; the ZIP CRC check passed.
- Nine local packaging tests passed, repository validation passed, and a fresh
  Windows Tape VST3 build passed with the shared CMake defaults.

The installer remains unsigned and unnotarized. No interactive DAW session on
Summer's Mac has been observed; Pro Tools/AAX and macOS versions other than the
tested version remain outside the demonstrated host coverage.
