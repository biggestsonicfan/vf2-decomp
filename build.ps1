<#
.SYNOPSIS
Convenience wrapper around cmake/ninja for vf2-decomp on this machine.
No elevated prompt required.

USAGE
  .\build.ps1 cfg            fresh cmake configure into build/
                             (auto-enables ROM-backed tests via roms\vf2)
  .\build.ps1 build          configure-if-needed + compile (strict Werror)
  .\build.ps1 test           run all CTest targets (ROM-backed when present)
  .\build.ps1 asan           ASAN+UBSan build into build_asan/
  .\build.ps1 trace [csv]    trace-orchestrator over $Repo\roms\vf2
  .\build.ps1 clean          wipe build/ and build_asan/

The default ROM directory is $Repo\roms\vf2. Override per-invocation with
the VF2_ROM_DIR environment variable, or skip ROM-backed tests by setting
VF2_ROM_DIR to an empty string.
#>
param(
  [ValidateSet('cfg','build','clean','test','asan','trace','help')]
  [string]$Command = 'help',
  [string[]]$Args
)

$Repo     = $PSScriptRoot
$BuildDir = Join-Path $Repo 'build'
$SanDir   = Join-Path $Repo 'build_asan'
$RomDir   = if ($env:VF2_ROM_DIR) { $env:VF2_ROM_DIR } else { Join-Path $Repo 'roms\vf2' }

# CMake accepts an empty -DVF2_ROM_DIR= as "unset", which falls back to the
# built-in default in CMakeLists.txt (also $Repo\roms\vf2). We only forward the
# flag when the directory actually exists so the configure step stays clean on
# machines without ROMs. Empty entries are filtered out of the splat below.
# Build a -D argument from a name and value. No inner quotes — they survive
# PowerShell splatting as literal characters and confuse CMake's -D parser
# on Windows. ROM paths under MSYS2 won't contain spaces.
function New-CMakeDefine {
  param([string]$Name, [string]$Value)
  if ($Value) { "-D${Name}=$Value" } else { '' }
}

# Wrap a list of strings into a real List[string] so splatting emits each
# element as one argv token rather than enumerating a scalar string's
# characters (PowerShell unwraps single-element pipelines to a scalar).
function New-ArgList {
  param([string[]]$Items)
  $list = [System.Collections.Generic.List[string]]::new()
  foreach ($e in $Items) { if ($e) { $list.Add($e) } }
  ,$list
}

# --- self-bootstrap: prepend the local toolchains this machine happens to have
#     (MSYS2 UCRT64 GCC + LLVM). No-op if they are absent.
$toolchains = @('C:\msys64\ucrt64\bin','C:\Program Files\LLVM\bin')
foreach ($tc in $toolchains) {
  if (Test-Path $tc) { $env:PATH = "$tc;$env:PATH" }
}
if (-not $env:CC -and (Get-Command gcc -ErrorAction SilentlyContinue)) { $env:CC = 'gcc' }
# MSYS2 UCRT64 GCC silently exits non-zero without MSYSTEM=UCRT64 (it depends on
# the per-environment host triplet to find its own runtime and libstdc++). Set
# it explicitly when the ucrt64 toolchain is present so the build works even
# from a non-MSYS2 PowerShell session.
if (Test-Path 'C:\msys64\ucrt64\bin') { $env:MSYSTEM = 'UCRT64' }

function Need-RomDir {
  if (-not (Test-Path "$RomDir")) {
    throw "$RomDir missing. Populate $Repo\roms\vf2 with the 36 VF2 ROMs and re-run."
  }
}

switch ($Command) {
  'cfg' {
    $romDefine = if (Test-Path $RomDir) { New-CMakeDefine 'VF2_ROM_DIR' $RomDir } else { '' }
    $extra = New-ArgList $romDefine
    & cmake -S "$Repo" -B "$BuildDir" -G Ninja `
      -DCMAKE_BUILD_TYPE=Release `
      -DVF2_BUILD_TESTS=ON `
      -DVF2_WARNINGS_AS_ERRORS=ON `
      @extra
  }

  'build' {
    if (-not (Test-Path "$BuildDir\build.ninja")) {
      $romDefine = if (Test-Path $RomDir) { New-CMakeDefine 'VF2_ROM_DIR' $RomDir } else { '' }
      $extra = New-ArgList $romDefine
      & cmake -S "$Repo" -B "$BuildDir" -G Ninja `
        -DCMAKE_BUILD_TYPE=Release `
        -DVF2_BUILD_TESTS=ON `
        -DVF2_WARNINGS_AS_ERRORS=ON `
        @extra
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
      $romDefine = if (Test-Path $RomDir) { New-CMakeDefine 'VF2_ROM_DIR' $RomDir } else { '' }
      $extra = New-ArgList $romDefine
      & cmake -S "$Repo" -B "$SanDir" -G Ninja `
        -DCMAKE_BUILD_TYPE=Debug `
        -DVF2_BUILD_TESTS=ON `
        -DVF2_WARNINGS_AS_ERRORS=ON `
        -DVF2_ENABLE_SANITIZERS=ON `
        @extra
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
                             (auto-passes -DVF2_ROM_DIR=roms\vf2 if present)
  .\build.ps1 build          configure-if-needed + compile (strict Werror)
  .\build.ps1 test           run all CTest targets (ROM-backed when present)
  .\build.ps1 asan           ASAN+UBSan build into build_asan\
  .\build.ps1 trace [csv]    trace-orchestrator over $Repo\roms\vf2
  .\build.ps1 clean          wipe build\ and build_asan\

  ROM dir defaults to $Repo\roms\vf2; override with $env:VF2_ROM_DIR.
'@ | Write-Host
  }
}
