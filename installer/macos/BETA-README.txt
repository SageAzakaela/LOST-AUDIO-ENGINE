B&E DIGITAL - MAC PLUGIN DEVELOPER PREVIEW

13 effects and creative tools, each supplied as VST3 and Audio Units (AU).
Every bundle contains native Apple silicon and Intel code.
Build target: macOS 11 Big Sur or later. Older macOS versions are not supported.
The build target is not proof of testing on every macOS version.

This preview has development signatures only. It is NOT Apple notarized.
Keep it for developer testing until a signed and notarized installer is ready.
If macOS blocks a downloaded plugin, stop and request the notarized installer.
Do not disable Gatekeeper or change the Mac's system security settings.

FORMATS
Logic Pro / GarageBand: use the .component files in Components (Audio Units).
Other DAWs with VST3 support: use the .vst3 files in VST3.
Pro Tools: requires AAX, which is not included or supported by this package.
Choose one format per DAW session to avoid duplicate plugin entries.

DEVELOPER INSTALLATION
Close the DAW first. Keep each plugin bundle intact.
VST3 folder: ~/Library/Audio/Plug-Ins/VST3/
AU folder:   ~/Library/Audio/Plug-Ins/Components/
In Finder, Go > Go to Folder opens these locations. Create missing folders.
Back up any previous version before replacing a same-name plugin.
Reopen the DAW, enable the relevant format and rescan plugins if necessary.
If a newly installed AU is not listed, log out and back in, then rescan.
Find the plugins under B&E Digital in the audio effects list.

FIRST SESSION
Use a copy of a project. Insert an effect on an audio track and start playback.
Try a factory preset, adjust the controls, then save, close and reopen the project.
Report the Mac model, macOS version, DAW/version, plugin/format and what happened.

VALIDATION
The accompanying manifest identifies the exact source and binary hashes.
Automated Apple AU validation and pluginval cover both CPU families.
Real DAW listening, automation and project-reopen checks remain separate QA.
These native plugins are in beta and have not established complete sonic parity
with the browser edition.
