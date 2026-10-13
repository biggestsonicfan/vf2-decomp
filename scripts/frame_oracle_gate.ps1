<#
.SYNOPSIS
Phase 2 gate: recovered native runtime vs i960 interpreter oracle.

From the ROM alone (cold snapshot) run the recovered native runtime and an
independent pure-i960 interpreter (vf2_host --oracle) from the same start
state, and require their per-frame work-RAM digests to be identical wherever
they jointly sample.

The two steppers define a frame differently: the native runtime injects a
frame interrupt at both the early-wait (0x0f7c) and the main-wait (0x10f98)
entry per visual frame, while the fixed-threshold oracle scheduler reaches its
visit threshold only at the main wait. The native side therefore emits two
digests for each oracle digest, and the states coincide exactly at the
main-wait boundary: oracle frame k equals native frame 2k-1. This gate checks
that correspondence for every frame.

.EXAMPLE
  .\scripts\frame_oracle_gate.ps1 .\roms\vf2 3600

Needs a build with -DVF2_BUILD_HOST=ON in build\ (vf2_host, vf2i960).
#>
param(
  [Parameter(Mandatory = $true)][string]$RomDir,
  [int]$OracleFrames = 3600
)
$ErrorActionPreference = 'Stop'
$Repo = Split-Path -Parent $PSScriptRoot
$NativeFrames = $OracleFrames * 2
$Host3 = Join-Path $Repo 'build\vf2_host.exe'
$out = Join-Path ([System.IO.Path]::GetTempPath()) ("vf2_frame_oracle_gate." + [System.Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $out | Out-Null
try {
  $snap = Join-Path $out 'cold.vf2snap'
  & (Join-Path $Repo 'scripts\cold_snapshot.ps1') $RomDir $snap | Out-Null
  $env:SDL_VIDEODRIVER = 'dummy'; $env:SDL_AUDIODRIVER = 'dummy'
  $native = & $Host3 --rom-dir $RomDir --snapshot $snap --frames $NativeFrames --digest 1 --fast 2>&1 |
    Select-String '^digest' | ForEach-Object { ($_ -split '\s+')[3] }
  $oracle = & $Host3 --rom-dir $RomDir --snapshot $snap --frames $OracleFrames --digest 1 --fast --oracle 2>&1 |
    Select-String '^digest' | ForEach-Object { ($_ -split '\s+')[3] }
  for ($k = 1; $k -le $oracle.Count; $k++) {
    $ni = 2 * $k - 1
    if ($ni -gt $native.Count) { throw "native run too short at oracle frame $k" }
    if ($oracle[$k - 1] -ne $native[$ni - 1]) {
      throw "MISMATCH oracle frame ${k}: oracle $($oracle[$k-1]) native[$ni] $($native[$ni-1])"
    }
  }
  $commit = (& git -C $Repo rev-parse --short HEAD).Trim()
  Write-Host "frame_oracle_gate: $($oracle.Count) oracle frames, all equal native[2k-1] (commit $commit)"
}
finally {
  Remove-Item -Recurse -Force $out -ErrorAction SilentlyContinue
}
