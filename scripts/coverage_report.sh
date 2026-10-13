#!/bin/sh
# Phase 3 coverage report, reproducible from the ROM alone.
#
# Builds the cold snapshot, records the attract session with the interpreter
# oracle (vf2i960 coverage), and runs tools/python/coverage_report.py against
# decomp/i960/functions.csv, refreshing the committed ranked backlog
# (addresses and hit counts only -- no ROM, snapshot or trace).
#
# Usage: scripts/coverage_report.sh /path/to/vf2 [frames=3600]
# Needs a build in build/ (vf2i960) and python3.
set -e
ROMS=${1:?usage: coverage_report.sh /path/to/vf2 [frames]}
FRAMES=${2:-3600}
cd "$(dirname "$0")/.."
BIN=${VF2I960:-build/vf2i960}
[ -x "$BIN" ] || { echo "build first: cmake --build build (looked for $BIN)" >&2; exit 1; }
mkdir -p out
"$BIN" native-sixth-dispatch "$ROMS" out/cold.vf2snap >/dev/null
# Sessions reachable from the cold corridor.  The attract spin does not yet
# poll coin (black frame; the interactive attract needs the geometry pipeline,
# plan phase 6), so only the attract session is recorded here.  Add more
# --session lines as later phases let the game advance (coin, select, match).
"$BIN" coverage "$ROMS" out/cold.vf2snap "$FRAMES" out/cov_attract.json
python tools/python/coverage_report.py \
    --session attract=out/cov_attract.json \
    --backlog-out decomp/i960/coverage_attract_backlog.csv
