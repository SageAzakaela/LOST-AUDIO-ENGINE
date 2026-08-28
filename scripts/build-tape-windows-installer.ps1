[CmdletBinding()]
param(
    [string] $BuildDirectory = (Join-Path $PSScriptRoot '..\build-tape'),
    [string] $OutputDirectory = (Join-Path $PSScriptRoot '..\dist\windows'),
    [string] $IsccPath = (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe')
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$resolvedBuild = [System.IO.Path]::GetFullPath($BuildDirectory)
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
$bundle = Join-Path $resolvedBuild 'tape-vst3\TapeEngine_artefacts\Release\VST3\Tape Engine.vst3'
$processor = Join-Path $bundle 'Contents\x86_64-win\Tape Engine.vst3'
$definition = Join-Path $repositoryRoot 'installer\windows\TapeEngine.iss'

if (-not (Test-Path -LiteralPath $IsccPath -PathType Leaf)) {
    throw "Inno Setup compiler was not found at: $IsccPath"
}
if (-not (Test-Path -LiteralPath $definition -PathType Leaf)) {
    throw "Installer definition was not found at: $definition"
}
if (-not (Test-Path -LiteralPath $bundle -PathType Container)) {
    throw "Tape Engine VST3 bundle was not found at: $bundle"
}
if (-not (Test-Path -LiteralPath $processor -PathType Leaf)) {
    throw "Tape Engine processor binary was not found at: $processor"
}

New-Item -ItemType Directory -Path $resolvedOutput -Force | Out-Null

& $IsccPath "/DBundleDir=$bundle" "/DOutputDir=$resolvedOutput" $definition
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE"
}

$installer = Get-ChildItem -LiteralPath $resolvedOutput -Filter 'Lost-Audio-Tape-Engine-*-Setup.exe' |
    Sort-Object LastWriteTimeUtc -Descending |
    Select-Object -First 1
if ($null -eq $installer) {
    throw "Installer output was not produced in: $resolvedOutput"
}

$hash = Get-FileHash -Algorithm SHA256 -LiteralPath $installer.FullName
$hashFile = "$($installer.FullName).sha256"
"$($hash.Hash.ToLowerInvariant())  $($installer.Name)" | Set-Content -LiteralPath $hashFile -Encoding ascii

Write-Output "INSTALLER=$($installer.FullName)"
Write-Output "SHA256=$($hash.Hash)"
