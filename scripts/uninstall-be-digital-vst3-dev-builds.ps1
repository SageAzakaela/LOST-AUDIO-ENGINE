param()

$ErrorActionPreference = 'Stop'

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
$isAdministrator = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdministrator) {
    $arguments = @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', ('"{0}"' -f $PSCommandPath)
    )
    $process = Start-Process -FilePath 'powershell.exe' -Verb RunAs -WindowStyle Hidden -ArgumentList $arguments -Wait -PassThru
    exit $process.ExitCode
}

$liveProcesses = Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $_.ProcessName -like 'Ableton Live*' -or
    ($_.Path -and $_.Path -match '\\Ableton Live [^\\]+\\Program\\Ableton Live')
}
if ($liveProcesses) {
    throw 'Ableton Live is still running. Close it before unloading the B&E Digital VST3 fleet.'
}

$installRoot = [System.IO.Path]::GetFullPath('C:\Program Files\Common Files\VST3')
$backupRoot = Join-Path $env:LOCALAPPDATA ('B&E Digital\VST3 Backups\unloaded-{0:yyyyMMdd-HHmmss}' -f (Get-Date))
$plugins = @(
    'Brain Cruncher',
    'Transmission Engine',
    'Tape Engine',
    'Television Engine',
    'Cartridge Engine',
    'CD Engine',
    'Comms Engine',
    'Conference Engine',
    'Camcorder Engine',
    'Occlusion Engine',
    'Open Mic Night',
    'Lost Audio Suite',
    'Lost Audio Sequencer'
)

if ($installRoot -ne 'C:\Program Files\Common Files\VST3') {
    throw "Unexpected VST3 source directory: $installRoot"
}

$targets = foreach ($name in $plugins) {
    $path = Join-Path $installRoot ("{0}.vst3" -f $name)
    $resolved = [System.IO.Path]::GetFullPath($path)
    $prefix = $installRoot.TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Uninstall target escaped the VST3 directory: $resolved"
    }
    if (-not (Test-Path -LiteralPath $resolved -PathType Container)) {
        throw "Expected installed B&E Digital bundle is missing: $resolved"
    }
    [pscustomobject]@{ Name = $name; Path = $resolved }
}

New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null

foreach ($target in $targets) {
    $destination = Join-Path $backupRoot (Split-Path -Leaf $target.Path)
    Move-Item -LiteralPath $target.Path -Destination $destination
    if ((Test-Path -LiteralPath $target.Path) -or -not (Test-Path -LiteralPath $destination -PathType Container)) {
        throw "Failed to unload and preserve $($target.Name)"
    }
    Write-Output ("UNLOADED {0}" -f $target.Name)
}

Set-Content -LiteralPath (Join-Path $backupRoot 'UNLOAD-MANIFEST.txt') -Value @(
    "Unloaded: $(Get-Date -Format o)",
    "Source: $installRoot",
    "Bundles: $($plugins.Count)",
    $plugins
)

Write-Output "BACKUP $backupRoot"
Write-Output 'DONE Launch Ableton once with the B&E Digital fleet absent.'
