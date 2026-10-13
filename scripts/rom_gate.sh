#!/bin/sh
# ROM-backed gate. Usage: scripts/rom_gate.sh /path/to/vf2 [ctest -R regex]
# Configures a separate build tree with the ROM directory, builds, runs the
# tests at -j2 (the heavy differentials need a lot of memory) and prints a
# one-line summary to paste into the commit message.
set -e
ROMS=${1:?usage: rom_gate.sh /path/to/vf2 [regex]}
RE=${2:-.}
cd "$(dirname "$0")/.."
cmake -S . -B build-rom -G Ninja -DCMAKE_BUILD_TYPE=Release -DVF2_BUILD_TESTS=ON \
      -DVF2_WARNINGS_AS_ERRORS=ON -DVF2_ROM_DIR="$ROMS" >/dev/null
cmake --build build-rom --parallel
set +e
ctest --test-dir build-rom -j2 -R "$RE" --output-on-failure > build-rom/gate.log 2>&1
RC=$?
tail -n 12 build-rom/gate.log
printf 'rom_gate: %s (ctest -R "%s", rc=%d, commit %s)\n' \
  "$(grep -E 'tests passed' build-rom/gate.log | tail -n 1)" "$RE" "$RC" "$(git rev-parse --short HEAD)"
exit $RC
