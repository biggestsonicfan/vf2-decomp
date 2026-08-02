param(
    [string]$RomDirectory = "roms/vf2",
    [string]$BuildDirectory = "build"
)

$ErrorActionPreference = "Stop"

& "$BuildDirectory/vf2rom.exe" verify $RomDirectory
& "$BuildDirectory/vf2i960.exe" compare-boot $RomDirectory
