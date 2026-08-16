#!/usr/bin/env sh
set -eu

ROM_DIR="${1:-roms/vf2}"
BUILD_DIR="${2:-build}"
DISPATCH_COUNT="${3:-256}"
SNAPSHOT="${4:-dispatch-${DISPATCH_COUNT}.vf2snap}"

"${BUILD_DIR}/vf2rom" verify "${ROM_DIR}"
"${BUILD_DIR}/vf2i960" native-nth-dispatch \
  "${ROM_DIR}" "${DISPATCH_COUNT}" "${SNAPSHOT}"
