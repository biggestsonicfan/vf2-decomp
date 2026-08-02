$ErrorActionPreference = "Stop"

cmake -S . -B build -DVF2_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure

Write-Host ""
Write-Host "Build complete."
Write-Host "Next: .\build\Release\vf2rom.exe verify .\roms\vf2"
