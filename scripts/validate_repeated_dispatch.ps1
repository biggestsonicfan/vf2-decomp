param(
    [string]$RomDir = "roms/vf2",
    [string]$BuildDir = "build",
    [uint32]$DispatchCount = 256,
    [string]$Snapshot = ""
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($Snapshot)) {
    $Snapshot = "dispatch-$DispatchCount.vf2snap"
}

& "$BuildDir/vf2rom" verify $RomDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& "$BuildDir/vf2i960" native-nth-dispatch $RomDir $DispatchCount $Snapshot
exit $LASTEXITCODE
