# Same as cold_snapshot.sh. Usage: scripts\cold_snapshot.ps1 <rom-dir> [out.vf2snap]
param([Parameter(Mandatory=$true)][string]$Roms, [string]$Out = "out\cold.vf2snap")
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")
$bin = if ($env:VF2I960) { $env:VF2I960 } else { "build\Release\vf2i960.exe" }
if (-not (Test-Path $bin)) { throw "build first (looked for $bin)" }
New-Item -ItemType Directory -Force -Path (Split-Path $Out) | Out-Null
& $bin native-sixth-dispatch $Roms $Out | Select-Object -Last 6
Write-Host "snapshot: $Out"
