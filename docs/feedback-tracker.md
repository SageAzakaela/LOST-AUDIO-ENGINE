# Lost Audio Engine feedback tracker

Last checked: 2026-08-27 (America/Chicago)

This is the evidence-linked intake for public requests, bug reports, and useful adoption signals. A request being recorded here does not mean it has been accepted or scheduled.

The broader accepted product direction is maintained in the [product roadmap](roadmap.md): platform integration, web-reference DSP parity, Device Engine V2 overhauls, Lost Audio Suite, and Lost Audio Sequencer.

## Recommended next work

| Priority | ID | Work item | Why it is next | Status |
| --- | --- | --- | --- | --- |
| P0 | LAE-001 | Synchronize the Camcorder plugin-ID fix back into source | The published March 1 Camcorder VST3 identifies itself as `CcEg`, while the restored source incorrectly reused Comms' `CmEg`. The source is now corrected to `CcEg`; a rebuilt artifact still needs validation against the published class IDs. | In progress; source reconciled |
| P0 | LAE-002 | Make releases reproducible | A root CMake entry point now configures all ten plugins against one `JUCE_DIR`, and the published ZIP set is hashed. JUCE is still unpinned and there is no CI/release workflow or clean rebuild proof. | In progress |
| P1 | LAE-003 | Produce and validate Linux VST3 and Standalone builds | This is the only direct request on the itch community. The JUCE/CMake code is plausibly portable, but a Linux build and host smoke test are still required before promising a release. | Requested; acknowledged by developer |
| P1 | LAE-004 | Produce and validate macOS VST3 and Standalone builds | A user requested macOS in the public launch thread. The JUCE/CMake code is plausibly portable, but macOS builds, Intel/Apple Silicon coverage, host testing, signing, and installation instructions remain unverified. | Requested; not committed |
| P1 | LAE-005 | Choose and publish a source license | The repository has no `LICENSE` file. “Free” downloads do not by themselves grant permission to modify or redistribute the source, which makes the invitation to build on it unclear. | Decision needed |
| P1 | LAE-006 | Establish one canonical feedback intake | GitHub Issues are enabled but empty, Discussions are disabled, and the itch community is where the actionable request appeared. The local source now links GitHub Issues and includes structured bug/request forms; publishing them and linking the itch page remain. | In progress locally |
| P2 | LAE-007 | Refresh README and release inventory | The README is Windows-only, omits Occlusion Engine, has no source-build instructions, and does not state platform support or where to report problems. | Proposed |
| P0 | LAE-008 | Preserve the stronger web sound as the parity reference | The web versions are the user-identified sonic reference and contain richer processing/preset definitions than the native plugins. Deterministic multi-rate fixtures and comparison tooling now exist; matched web renders still need capture. | In progress |
| P1 | LAE-009 | Overhaul every Device Engine | Port approved web-reference behavior, then apply the shared Surface/Advanced UX, visual system, deeper presets, metering, smoothing, resizing, and QA standard. | Accepted; Transmission first |
| P1 | LAE-010 | Build Lost Audio Suite | Create one native plugin with reorderable slots for the complete validated effect collection and shared chain presets. | Accepted; follows shared core |
| P1 | LAE-011 | Build Lost Audio Sequencer | Create an original tempo-synced step-effects instrument with probability, macro locks, pattern recall, safe transitions, and deterministic behavior. | Accepted; follows Suite/core validation |
| P1 | LAE-012 | Redesign the web workstation | The first local pass removes the horizontal mega-grid, strengthens hierarchy/device identity, expands focused views, and fixes the 540 px layout. The complete selected-device workstation, preset rail, waveform, and automation redesign remain. | In progress |
| P0 | LAE-013 | Prevent abrasive startup audio | Testing found that a fresh web session implicitly enabled Transmission at 100% wet with its noise, crackle, dropout, and saturation defaults. Startup now explicitly applies `Safety Clean`; effects remain opt-in through engine enables or presets. | Fixed locally; retest requested |

## Public requests and signals

| Record | Date | Source | Type | What was said | Current interpretation | Status |
| --- | --- | --- | --- | --- | --- | --- |
| ITCH-001 | 2026-07-26 | [itch topic: “awesome tools”](https://itch.io/t/6704337/awesome-tools) | Platform request | haetae asked whether the tools could be made available on Linux. Azakaela replied, “yes! I'll see what I can do.” | Direct demand for Linux releases. This is an acknowledgement, not a completed commitment. | Open → LAE-003 |
| ITCH-002 | 2026-02-11 | [itch topic: “Holy cow”](https://itch.io/t/5921830/holy-cow) | Adoption signal | AmalgamAsh praised the tool and said they planned to demo/share it on their channel. | Promotion and enthusiasm; no product work requested. | No action |
| REDDIT-001 | 2026-02-27 | [r/sounddesign launch thread](https://www.reddit.com/r/sounddesign/comments/1rfn1ry/lost_audio_engine_by_azakaela/) | Platform request | A commenter asked whether there would be a macOS version. The reply said macOS deployment needed investigation. | Direct demand for macOS releases, found outside GitHub/itch while tracing public conversation. | Open → LAE-004 |
| GH-000 | 2026-08-27 | [GitHub repository](https://github.com/SageAzakaela/LOST-AUDIO-ENGINE) | Verified empty intake | Zero issues, zero pull requests, Discussions disabled, zero commit comments, zero forks, and no exact repository-URL matches in GitHub issue comments or indexed code. | There is no currently verifiable GitHub feature request to import. If a notification or URL surfaces later, add it without guessing. | Recheck later |

## Repository evidence affecting the backlog

- The repository was last pushed on 2026-02-26, before the [March 1 Camcorder replacement](https://azakaela.itch.io/lost-audio-engine/devlog/1433553/replaced-camcorder-vst).
- The published Camcorder component CID decodes to plugin code `CcEg`; Comms uses `CmEg`. The restored Camcorder CMake definition has been reconciled to `CcEg`, with exact artifact evidence recorded in the [release audit](release-audit.md).
- All plugin projects already request `FORMATS VST3 Standalone` through JUCE. This supports investigating Linux and macOS, but it is not proof that the current code builds or behaves correctly there.
- There is no root build orchestrator, pinned JUCE dependency, automated build workflow, release validation, or source license.
- GitHub’s current repository counters are 1 star, 0 forks, 0 open issues, and 0 subscribers. These counters are context, not a measure of itch adoption.

## Tracking rules

- Preserve the submitter’s actual request and link the original source.
- Keep evidence, interpretation, and proposed work separate.
- Do not mark a request complete until a downloadable artifact and at least one host smoke test are verified on that platform.
- Use `Open`, `Accepted`, `In progress`, `Blocked`, `Released`, `Declined`, or `No action` for request status.
- When itch and GitHub describe the same request, retain the itch record but make one GitHub issue the canonical implementation thread.

## Refresh checklist

1. Check [itch community topics](https://azakaela.itch.io/lost-audio-engine/community), including replies to devlogs.
2. Check GitHub Issues, pull requests, commit comments, and—if enabled later—Discussions.
3. Search for exact links to the itch page and repository before treating similarly named audio projects as related.
4. Add only newly verified records, update statuses, and change the “Last checked” date.
