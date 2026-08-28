# Lost Audio Engine published-release audit

Captured: 2026-08-27 (America/Chicago)
Source: [Lost Audio Engine itch download page](https://azakaela.itch.io/lost-audio-engine)

The ten published Windows VST3 ZIP files were downloaded into the ignored local directory `.artifacts/itch-vst3-2026-08-27/`. This manifest is tracked; the large release binaries are intentionally not added to Git.

## Artifact manifest

| Artifact | Bytes | SHA-256 | Component CID | Plugin code | Published |
| --- | ---: | --- | --- | --- | --- |
| Camcorder Engine.vst3.zip | 2,745,222 | `3A6C3271F1670AD36BE5B6C11B0DE6BF0983DD57890EC866B62E22CFEF709FE4` | `5653544363456763616D636F72646572` | `CcEg` | 2026-03-01 |
| Cartridge Engine.vst3.zip | 2,746,307 | `628EFF1961EF1F71A9524DFF125782A3D9F13DCC3D65C765DAACBEFC3060541C` | `56535443724567636172747269646765` | `CrEg` | 2026-02-26 |
| CD Engine.vst3.zip | 2,334,153 | `D6EA184FC579B029E3D6314965ED003889C01D84DBA7EEABADB96017688C4187` | `56535443644567636420656E67696E65` | `CdEg` | 2026-02-26 |
| Comms Engine.vst3.zip | 2,744,641 | `4AA01ABEE4D20785595C7D399A012923779A49655D379BAE3EA2AACC06F19CEA` | `565354436D4567636F6D6D7320656E67` | `CmEg` | 2026-02-26 |
| Conference Engine.vst3.zip | 2,739,449 | `8AA54F2A01F6930097EBA0A5625952FE136A9210DD4CCCBB2FF71D5C1A27BD2A` | `56535443664567636F6E666572656E63` | `CfEg` | 2026-02-26 |
| Occlusion Engine.vst3.zip | 2,739,353 | `9CCF8A2FDA155CB0ACE9B413F196F3FAC25062FF7C1C99927538F64682289C92` | `5653544F6345676F63636C7573696F6E` | `OcEg` | 2026-04-14 |
| Open Mic Night.vst3.zip | 43,224,046 | `D6641E20D431E3F331A5D18F17FADE573669A347223415AC32789B49AAD752D9` | `5653544F6D4E746F70656E206D696320` | `OmNt` | 2026-02-26 |
| Tape Engine.vst3.zip | 16,416,420 | `128EB0000963A566F33D5DEE9AC9B5EBE747C38A439F4F5372024E98B4387E1C` | `565354547045677461706520656E6769` | `TpEg` | 2026-02-26 |
| Television Engine.vst3.zip | 3,650,326 | `40380DE0DA42724D4C9A24C20B79DC656559070703A0953DC036F7609724546F` | `5653545476456774656C65766973696F` | `TvEg` | 2026-02-26 |
| Transmission Engine.vst3.zip | 8,571,664 | `2817F19DB55416E9E8304F7F6E568DE8AD407E497EE0433734338EAB5D6A5BE6` | `56535454726E457472616E736D697373` | `TrnE` | 2026-02-26 |

All embedded `moduleinfo.json` files report plugin version `0.1.0` and VST SDK `3.8.0`.

## Camcorder reconciliation

The March 1 replacement's component CID begins with the ASCII identity `VSTCcEg`, while the February Comms component begins with `VSTCmEg`. This is direct evidence that the released Camcorder fix changed its four-character JUCE `PLUGIN_CODE` from the conflicting `CmEg` to `CcEg`.

The restored source has therefore been changed to `PLUGIN_CODE CcEg`. LAE-001 is not fully closed until a fresh Camcorder build produces the same component and controller CIDs and both Camcorder and Comms load together in a host without replacing one another.

## Remaining preservation work

- Capture the deployed web build separately from the newer local web source.
- Record ZIP entry hashes if exact bundle-level forensics becomes necessary.
- Rebuild from source and compare identity, state recall, presets, and representative audio behavior.
