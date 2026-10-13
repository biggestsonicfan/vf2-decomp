#!/bin/sh
# Builds a start state from the ROM alone (no MAME, no prior snapshot):
# resets the i960 and runs the validated native corridor to the sixth
# dispatch, then writes the snapshot that vf2_host --snapshot accepts.
# Usage: scripts/cold_snapshot.sh /path/to/vf2 [out.vf2snap]   (about 6 s)
set -e
ROMS=${1:?usage: cold_snapshot.sh /path/to/vf2 [out.vf2snap]}
OUT=${2:-out/cold.vf2snap}
cd "$(dirname "$0")/.."
BIN=${VF2I960:-build/vf2i960}
[ -x "$BIN" ] || { echo "build first: cmake --build build (looked for $BIN)" >&2; exit 1; }
mkdir -p "$(dirname "$OUT")"
"$BIN" native-sixth-dispatch "$ROMS" "$OUT" | tail -n 6
echo "snapshot: $OUT"
