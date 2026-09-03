param(
    [string] $BuildDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build-tape-dev'),
    [string] $OutputDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'dist\dev\be-digital-vst3-windows-x64'),
    [string[]] $ExcludePlugins = @()
)

$ErrorActionPreference = 'Stop'
$resolvedBuild = [System.IO.Path]::GetFullPath($BuildDirectory)
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputDirectory)

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

foreach ($name in $ExcludePlugins) {
    if (-not $plugins.Contains($name)) {
        throw "Unknown plugin requested for exclusion: $name"
    }
    $plugins.Remove($name)
}

$resolved = foreach ($entry in $plugins.GetEnumerator()) {
    $bundle = Join-Path $resolvedBuild (Join-Path $entry.Value ("Release\VST3\{0}.vst3" -f $entry.Key))
    $binary = Join-Path $bundle ("Contents\x86_64-win\{0}.vst3" -f $entry.Key)
    if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
        throw "Missing VST3 binary: $binary"
    }
    [pscustomobject]@{ Name = $entry.Key; Bundle = $bundle; Binary = $binary }
}

New-Item -ItemType Directory -Path $resolvedOutput -Force | Out-Null
$hashes = foreach ($plugin in $resolved) {
    $destination = Join-Path $resolvedOutput ("{0}.vst3" -f $plugin.Name)
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Get-ChildItem -LiteralPath $plugin.Bundle | Copy-Item -Destination $destination -Recurse -Force
    $stagedBinary = Join-Path $destination ("Contents\x86_64-win\{0}.vst3" -f $plugin.Name)
    $sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $plugin.Binary).Hash
    $stagedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $stagedBinary).Hash
    if ($sourceHash -ne $stagedHash) { throw "Hash verification failed for $($plugin.Name)" }
    "{0} *{1}.vst3/Contents/x86_64-win/{1}.vst3" -f $sourceHash, $plugin.Name
    Write-Host ("VERIFIED {0}" -f $plugin.Name)
}

$hashes | Set-Content -LiteralPath (Join-Path $resolvedOutput 'SHA256SUMS.txt') -Encoding ascii
$individualCount = @($plugins.Keys | Where-Object { $_ -notin @('Lost Audio Suite', 'Lost Audio Sequencer') }).Count
$productDescription = if ($plugins.Contains('Lost Audio Suite') -and $plugins.Contains('Lost Audio Sequencer')) {
    "$individualCount individual B&E Digital effects, Lost Audio Suite, and Lost Audio Sequencer"
} else {
    "$($plugins.Count) B&E Digital VST3 products"
}

@"
B&E Digital VST3 developer QA fleet
Built: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss K')
Platform: Windows x64
Format: VST3
Products: $productDescription
Excluded: $(if ($ExcludePlugins.Count) { $ExcludePlugins -join ', ' } else { 'None' })

Unsigned local development build. Close the DAW before installing or replacing
bundles, then force a VST3 rescan. Audible character remains a hands-on QA gate.
"@ | Set-Content -LiteralPath (Join-Path $resolvedOutput 'DEV-BUILD.txt') -Encoding utf8

Write-Output "STAGED $resolvedOutput"
