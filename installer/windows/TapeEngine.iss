#define AppName "Lost Audio Engine - Tape Engine"
#define AppVersion "0.1.0"
#define AppPublisher "B&E Digital"
#define AppUrl "https://bande.digital/tools/lost/"

#ifndef BundleDir
  #error BundleDir must point to the compiled Tape Engine.vst3 bundle.
#endif

#ifndef OutputDir
  #define OutputDir "..\..\dist\windows"
#endif

[Setup]
AppId={{A6BD60D0-2767-4B53-B399-3A81372A106E}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion} Preview
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL=https://github.com/SageAzakaela/LOST-AUDIO-ENGINE/issues
AppUpdatesURL={#AppUrl}
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
DirExistsWarning=no
UninstallDisplayName={#AppName} {#AppVersion}
UninstallDisplayIcon={commoncf64}\VST3\Tape Engine.vst3\Contents\x86_64-win\Tape Engine.vst3
OutputDir={#OutputDir}
OutputBaseFilename=Lost-Audio-Tape-Engine-v{#AppVersion}-Windows-x64-Setup
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0.19041
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
SetupLogging=yes
UsePreviousAppDir=no
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} VST3 Installer
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}
VersionInfoVersion=0.1.0.0

[Files]
Source: "{#BundleDir}\*"; DestDir: "{commoncf64}\VST3\Tape Engine.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Messages]
WelcomeLabel2=This installs the 64-bit Tape Engine VST3 from Lost Audio Engine.%n%nClose your DAW before continuing so Windows can update the plug-in safely.
FinishedHeadingLabel=Tape Engine is installed
FinishedLabel=Your DAW can now scan Tape Engine from the standard Windows VST3 location.%n%nNo manual file moving is required.

[Code]
function InitializeSetup(): Boolean;
begin
  Result := IsWin64;
  if not Result then
    MsgBox('Tape Engine requires 64-bit Windows.', mbError, MB_OK);
end;
