<#
.SYNOPSIS
Phase 6 step 1: census of the geometry command stream, reproducible from the
ROM alone.

Builds the cold snapshot, then runs `vf2i960 geometry-census`, which observes
every 32-bit write into the geometry ring (0x00800000), the coprocessor port
(0x00880000) and the geometry buffer RAM (0x00900000) over N attract frames and
classifies each by command class = (word >> 23) & 0x1f, reporting per class the
count, distinct words, how many matched a known copro protocol immediate, and
sample header words.

On the reachable attract corridor this confirms (reproducibly, in-tree) the
v0375-v0378 finding: the stream is copro-FPU arithmetic (camera scalars) plus
ring bookkeeping, with no renderable TGP geometry opcodes -- the game does not
submit geometry until it advances past attract (docs/NATIVE_WINDOWS_PLAN.md
Phase 6 Status).

.EXAMPLE
  .\scripts\geometry_census.ps1 .\roms\vf2 600

Needs a build in build\ (vf2i960).
#>
param(
  [Parameter(Mandatory = $true)][string]$RomDir,
  [int]$Frames = 600
)
$ErrorActionPreference = 'Stop'
$Repo = Split-Path -Parent $PSScriptRoot
$Bin = Join-Path $Repo 'build\vf2i960.exe'
if (-not (Test-Path $Bin)) { throw "build first: cmake --build build (looked for $Bin)" }
New-Item -ItemType Directory -Force -Path (Join-Path $Repo 'out') | Out-Null
& $Bin native-sixth-dispatch $RomDir (Join-Path $Repo 'out\cold.vf2snap') | Out-Null
& $Bin geometry-census $RomDir (Join-Path $Repo 'out\cold.vf2snap') $Frames
