<#
.SYNOPSIS
Convenience wrapper around cmake/ninja for vf2-decomp on this machine.
No elevated prompt required.

USAGE
  .\build.ps1 cfg            fresh cmake configure into build/
  .\build.ps1 build          configure-if-needed + compile (strict Werror)
  .\build.ps1 test           run all 22 CTest targets (ROM-backed)
  .\build.ps1 asan           ASAN+UBSan build into build_asan/
  .\build.ps1 trace [csv]    trace-orchestrator over $Repo\roms\vf2
  .\build.ps1 clean          wipe build/ and build_asan/
#>
param(
  [ValidateSet('cfg','build','clean','test','asan','trace','help')]
  [string]$Command = 'help',
  [string[]]$Args
)

$Repo     = $PSScriptRoot
$BuildDir = Join-Path $Repo 'build'
$SanDir   = Join-Path $Repo 'build_asan'
$RomDir   = Join-Path $Repo 'roms\vf2'

# --- self-bootstrap: prepend the local toolchains this machine happens to have
#     (MSYS2 UCRT64 GCC + LLVM). No-op if they are absent.
$toolchains = @('C:\msys64\ucrt64\bin','C:\Program Files\LLVM\bin')
foreach ($tc in $toolchains) {
  if (Test-Path $tc) { $env:PATH = "$tc;$env:PATH" }
}
if (-not $env:CC -and (Get-Command gcc -ErrorAction SilentlyContinue)) { $env:CC = 'gcc' }

function Need-RomDir {
  if (-not (Test-Path "$RomDir")) {
    throw "$RomDir missing. Populate $Repo\roms\vf2 with the 36 VF2 ROMs and re-run."
  }
}

switch ($Command) {
  'cfg' {
    & cmake -S "$Repo" -B "$BuildDir" -G Ninja `
      -DCMAKE_BUILD_TYPE=Release `
      -DVF2_BUILD_TESTS=ON `
      -DVF2_WARNINGS_AS_ERRORS=ON
  }

  'build' {
    if (-not (Test-Path "$BuildDir\build.ninja")) {
      & cmake -S "$Repo" -B "$BuildDir" -G Ninja `
        -DCMAKE_BUILD_TYPE=Release `
        -DVF2_BUILD_TESTS=ON `
        -DVF2_WARNINGS_AS_ERRORS=ON
    }
    & cmake --build "$BuildDir"
  }

  'clean' {
    if (Test-Path "$BuildDir") { Remove-Item "$BuildDir" -Recurse -Force }
    if (Test-Path "$SanDir")  { Remove-Item "$SanDir"  -Recurse -Force }
    Write-Host "build dirs removed"
  }

  'test' {
    if (-not (Test-Path "$BuildDir\build.ninja")) {
      throw "run '.\\build.ps1 build' first"
    }
    & ctest --test-dir "$BuildDir" -C Release --output-on-failure
  }

  'asan' {
    if (-not (Test-Path "$SanDir\build.ninja")) {
      & cmake -S "$Repo" -B "$SanDir" -G Ninja `
        -DCMAKE_BUILD_TYPE=Debug `
        -DVF2_BUILD_TESTS=ON `
        -DVF2_WARNINGS_AS_ERRORS=ON `
        -DVF2_ENABLE_SANITIZERS=ON
    }
    & cmake --build "$SanDir"
  }

  'trace' {
    Need-RomDir
    if (-not (Test-Path "$BuildDir\vf2i960.exe")) {
      throw "run '.\\build.ps1 build' first"
    }
    $csv = if ($Args.Count -gt 0) { $Args[0] } else { (Join-Path $Repo 'decomp/i960/notes/texture_orchestrator_v0023.csv') }
    & "$BuildDir\vf2i960.exe" trace-orchestrator "$RomDir" "$csv"
  }

  'help' {
    @'
  vf2-decomp build harness (no admin needed)

  .\build.ps1 cfg            fresh cmake configure into build\
  .\build.ps1 build          configure-if-needed + compile (strict Werror)
  .\build.ps1 test           run all 22 CTest targets (ROM-backed)
  .\build.ps1 asan           ASAN+UBSan build into build_asan\
  .\build.ps1 trace [csv]    trace-orchestrator over $Repo\roms\vf2
  .\build.ps1 clean          wipe build\ and build_asan\
'@ | Write-Host
  }
}
