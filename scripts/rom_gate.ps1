# ROM-backed gate. Usage: scripts\rom_gate.ps1 -Roms D:\path\to\vf2 [-Regex pattern]
param([Parameter(Mandatory = $true)][string]$Roms, [string]$Regex = ".")
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")
cmake -S . -B build-rom -DVF2_BUILD_TESTS=ON -DVF2_WARNINGS_AS_ERRORS=ON "-DVF2_ROM_DIR=$Roms" | Out-Null
cmake --build build-rom --config Release --parallel
ctest --test-dir build-rom -C Release -j2 -R $Regex --output-on-failure 2>&1 | Tee-Object build-rom\gate.log | Select-Object -Last 12
$sum = (Select-String -Path build-rom\gate.log -Pattern "tests passed" | Select-Object -Last 1).Line
"rom_gate: $sum (ctest -R `"$Regex`", commit $(git rev-parse --short HEAD))"
