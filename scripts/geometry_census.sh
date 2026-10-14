#!/bin/sh
# Phase 6 step 1: census of the geometry command stream, reproducible from the
# ROM alone.
#
# Builds the cold snapshot, then runs `vf2i960 geometry-census`, which observes
# every 32-bit write into the geometry ring (0x00800000), the coprocessor port
# (0x00880000) and the geometry buffer RAM (0x00900000) over N attract frames
# and classifies each by command class = (word >> 23) & 0x1f, reporting per
# class the count, distinct words, how many matched a known copro protocol
# immediate, and sample header words.
#
# On the reachable attract corridor this confirms (reproducibly, in-tree) the
# v0375-v0378 finding: the stream is copro-FPU arithmetic (camera scalars) plus
# ring bookkeeping, with no renderable TGP geometry opcodes -- the game does not
# submit geometry until it advances past attract (docs/NATIVE_WINDOWS_PLAN.md
# Phase 6 Status).
#
# Usage: scripts/geometry_census.sh /path/to/vf2 [frames=600]
# Needs a build in build/ (vf2i960).
set -e
ROMS=${1:?usage: geometry_census.sh /path/to/vf2 [frames]}
FRAMES=${2:-600}
cd "$(dirname "$0")/.."
BIN=${VF2I960:-build/vf2i960}
[ -x "$BIN" ] || { echo "build first: cmake --build build (looked for $BIN)" >&2; exit 1; }
mkdir -p out
"$BIN" native-sixth-dispatch "$ROMS" out/cold.vf2snap >/dev/null
"$BIN" geometry-census "$ROMS" out/cold.vf2snap "$FRAMES"
