#!/bin/sh
# Phase 2 gate: from the ROM alone (cold snapshot), run N frames twice with the
# dummy SDL drivers and require identical work-RAM digests every 60 frames.
# Usage: scripts/frame_gate.sh /path/to/vf2 [frames=3600]
# Needs a build with -DVF2_BUILD_HOST=ON in build/ (vf2_host, vf2i960).
set -e
ROMS=${1:?usage: frame_gate.sh /path/to/vf2 [frames]}
FRAMES=${2:-3600}
cd "$(dirname "$0")/.."
OUT=${TMPDIR:-/tmp}/vf2_frame_gate.$$
mkdir -p "$OUT"
scripts/cold_snapshot.sh "$ROMS" "$OUT/cold.vf2snap" >/dev/null
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
for run in 1 2; do
  build/vf2_host --rom-dir "$ROMS" --snapshot "$OUT/cold.vf2snap" \
      --frames "$FRAMES" --digest 60 --fast 2>&1 | grep '^digest' > "$OUT/run$run.txt"
done
N=$(wc -l < "$OUT/run1.txt")
cmp "$OUT/run1.txt" "$OUT/run2.txt"
echo "frame_gate: $FRAMES frames, $N digests identical across 2 runs (commit $(git rev-parse --short HEAD))"
rm -rf "$OUT"
