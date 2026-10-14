# Parallel ctest for this checkout. The suite is parallel-safe
# (audited v0825: every shared path is a read of a stable input,
# a per-test tempdir, or a distinct best-effort WAV; no two tests
# write the same file), so jobs default to the machine's logical
# processor count. Forwards extra args to ctest verbatim.
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ExtraArgs
)

$jobs = $env:NUMBER_OF_PROCESSORS
if (-not $jobs -or $jobs -lt 1) {
    $jobs = 8
}
$cmd = @(
    "ctest", "--test-dir", "build", "-C", "Debug",
    "-j", "$jobs", "--output-on-failure"
) + $ExtraArgs
Write-Host ("Running: " + ($cmd -join " "))
& $cmd[0] $cmd[1..($cmd.Length - 1)]
exit $LASTEXITCODE
