#!/bin/sh
# Phase 2 gate: native runtime vs i960 interpreter oracle.
#
# From the ROM alone (cold snapshot) run the recovered native runtime and an
# independent pure-i960 interpreter (vf2_host --oracle) from the same start
# state, and require their per-frame work-RAM digests to be identical wherever
# they jointly sample.
#
# The two steppers define a frame differently: the native runtime injects a
# frame interrupt at both the early-wait (0x0f7c) and the main-wait (0x10f98)
# entry per visual frame, while the fixed-threshold oracle scheduler reaches
# its visit threshold only at the main wait.  The native side therefore emits
# two digests for each oracle digest, and the states coincide exactly at the
# main-wait boundary: oracle frame k equals native frame 2k-1.  This gate
# checks that correspondence for every frame.
#
# Usage: scripts/frame_oracle_gate.sh /path/to/vf2 [oracle-frames=3600]
# Needs a build with -DVF2_BUILD_HOST=ON in build/ (vf2_host, vf2i960).
set -e
ROMS=${1:?usage: frame_oracle_gate.sh /path/to/vf2 [oracle-frames]}
ORACLE_FRAMES=${2:-3600}
NATIVE_FRAMES=$((ORACLE_FRAMES * 2))
cd "$(dirname "$0")/.."
OUT=${TMPDIR:-/tmp}/vf2_frame_oracle_gate.$$
mkdir -p "$OUT"
scripts/cold_snapshot.sh "$ROMS" "$OUT/cold.vf2snap" >/dev/null
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
build/vf2_host --rom-dir "$ROMS" --snapshot "$OUT/cold.vf2snap" \
    --frames "$NATIVE_FRAMES" --digest 1 --fast 2>&1 \
    | awk '/^digest/ {print $4}' > "$OUT/native.txt"
build/vf2_host --rom-dir "$ROMS" --snapshot "$OUT/cold.vf2snap" \
    --frames "$ORACLE_FRAMES" --digest 1 --fast --oracle 2>&1 \
    | awk '/^digest/ {print $4}' > "$OUT/oracle.txt"
# oracle[k] (1-indexed) must equal native[2k-1]: native line 2k-1.
awk 'NR==FNR {n[FNR]=$0; next}
     {k=FNR; want=n[2*k-1];
      if (want=="") {print "native run too short at oracle frame " k; bad=1; exit 1}
      if ($0!=want) {print "MISMATCH oracle frame " k ": oracle " $0 " native[" (2*k-1) "] " want; bad=1; exit 1}
      ok++}
     END {if (!bad) printf "frame_oracle_gate: %d oracle frames, all equal native[2k-1] (commit %s)\n", ok, COMMIT}' \
    COMMIT="$(git rev-parse --short HEAD)" "$OUT/native.txt" "$OUT/oracle.txt"
rm -rf "$OUT"
