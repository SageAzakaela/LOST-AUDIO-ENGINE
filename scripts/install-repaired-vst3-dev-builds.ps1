param(
    [string] $BuildDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build-tape-dev')
)

$ErrorActionPreference = 'Stop'
$resolvedBuild = [System.IO.Path]::GetFullPath($BuildDirectory)

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
$isAdministrator = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdministrator) {
    $arguments = @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', ('"{0}"' -f $PSCommandPath),
        '-BuildDirectory', ('"{0}"' -f $resolvedBuild)
    )
    $process = Start-Process -FilePath 'powershell.exe' -Verb RunAs -ArgumentList $arguments -Wait -PassThru
    exit $process.ExitCode
}

$installRoot = [System.IO.Path]::GetFullPath('C:\Program Files\Common Files\VST3')
$backupRoot = Join-Path $env:LOCALAPPDATA ('B&E Digital\VST3 Backups\{0:yyyyMMdd-HHmmss}' -f (Get-Date))

if (-not (Test-Path -LiteralPath $resolvedBuild -PathType Container)) {
    throw "Build directory does not exist: $resolvedBuild"
}
if ($installRoot -ne 'C:\Program Files\Common Files\VST3') {
    throw "Unexpected VST3 destination: $installRoot"
}

$plugins = [ordered]@{
    'Brain Cruncher'      = 'brain-cruncher-vst3\BrainCruncher_artefacts'
    'Transmission Engine' = 'transmission-vst3\TransmissionEngine_artefacts'
    'Tape Engine'         = 'tape-vst3\TapeEngine_artefacts'
    'Television Engine'   = 'television-vst3\TelevisionEngine_artefacts'
    'Cartridge Engine'    = 'cartridge-vst3\CartridgeEngine_artefacts'
    'CD Engine'           = 'cd-vst3\CDEngine_artefacts'
    'Comms Engine'        = 'comms-vst3\CommsEngine_artefacts'
    'Conference Engine'   = 'conference-vst3\ConferenceEngine_artefacts'
    'Camcorder Engine'    = 'camcorder-vst3\CamcorderEngine_artefacts'
    'Occlusion Engine'    = 'occlusion-vst3\OcclusionEngine_artefacts'
    'Open Mic Night'      = 'openmicnight-vst3\OpenMicNight_artefacts'
    'Lost Audio Suite'    = 'suite-vst3\LostAudioSuite_artefacts'
    'Lost Audio Sequencer'= 'sequencer-vst3\LostAudioSequencer_artefacts'
}

$resolved = foreach ($entry in $plugins.GetEnumerator()) {
    $bundle = Join-Path $resolvedBuild (Join-Path $entry.Value ("Release\VST3\{0}.vst3" -f $entry.Key))
    $binary = Join-Path $bundle ("Contents\x86_64-win\{0}.vst3" -f $entry.Key)
    if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
        throw "Missing repaired VST3 binary: $binary"
    }
    [pscustomobject]@{ Name = $entry.Key; Bundle = $bundle; Binary = $binary }
}

New-Item -ItemType Directory -Path $installRoot -Force | Out-Null
New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null

foreach ($plugin in $resolved) {
    $destination = Join-Path $installRoot ("{0}.vst3" -f $plugin.Name)
    $resolvedDestination = [System.IO.Path]::GetFullPath($destination)
    $installPrefix = $installRoot.TrimEnd('\') + '\'
    if (-not $resolvedDestination.StartsWith($installPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Install target escaped the VST3 directory: $resolvedDestination"
    }

    if (Test-Path -LiteralPath $destination) {
        $backupDestination = Join-Path $backupRoot (Split-Path -Leaf $destination)
        $resolvedBackup = [System.IO.Path]::GetFullPath($backupDestination)
        $backupPrefix = [System.IO.Path]::GetFullPath($backupRoot).TrimEnd('\') + '\'
        if (-not $resolvedBackup.StartsWith($backupPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Backup target escaped the B&E backup directory: $resolvedBackup"
        }
        Move-Item -LiteralPath $destination -Destination $backupDestination
    }

    Copy-Item -LiteralPath $plugin.Bundle -Destination $destination -Recurse
    $installedBinary = Join-Path $destination ("Contents\x86_64-win\{0}.vst3" -f $plugin.Name)
    $sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $plugin.Binary).Hash
    $installedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $installedBinary).Hash
    if ($sourceHash -ne $installedHash) {
        throw "Hash verification failed for $($plugin.Name)"
    }
    Write-Output ("VERIFIED {0}" -f $plugin.Name)
}

Write-Output "BACKUP $backupRoot"
Write-Output 'DONE Close and reopen the DAW, then force a VST3 rescan.'
