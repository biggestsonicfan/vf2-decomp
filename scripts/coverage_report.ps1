<#
.SYNOPSIS
Phase 3 coverage report, reproducible from the ROM alone.

Builds the cold snapshot, records the attract session with the interpreter
oracle (vf2i960 coverage), and runs tools/python/coverage_report.py against
decomp/i960/functions.csv, refreshing the committed ranked backlog (addresses
and hit counts only -- no ROM, snapshot or trace).

The cold corridor is still the black-frame attract spin and does not yet poll
coin, so only the attract session is recorded; add more sessions as later
phases let the game advance.

.EXAMPLE
  .\scripts\coverage_report.ps1 .\roms\vf2 3600

Needs a build in build\ (vf2i960) and python.
#>
param(
  [Parameter(Mandatory = $true)][string]$RomDir,
  [int]$Frames = 3600
)
$ErrorActionPreference = 'Stop'
$Repo = Split-Path -Parent $PSScriptRoot
$Bin = Join-Path $Repo 'build\vf2i960.exe'
if (-not (Test-Path $Bin)) { throw "build first: cmake --build build (looked for $Bin)" }
New-Item -ItemType Directory -Force -Path (Join-Path $Repo 'out') | Out-Null
& $Bin native-sixth-dispatch $RomDir (Join-Path $Repo 'out\cold.vf2snap') | Out-Null
& $Bin coverage $RomDir (Join-Path $Repo 'out\cold.vf2snap') $Frames (Join-Path $Repo 'out\cov_attract.json')
& python (Join-Path $Repo 'tools\python\coverage_report.py') `
    --session ("attract=" + (Join-Path $Repo 'out\cov_attract.json')) `
    --backlog-out (Join-Path $Repo 'decomp\i960\coverage_attract_backlog.csv')
