#!/usr/bin/env sh
set -eu

ROM_DIR="${1:-roms/vf2}"
BUILD_DIR="${2:-build}"

"${BUILD_DIR}/vf2rom" verify "${ROM_DIR}"
"${BUILD_DIR}/vf2i960" compare-boot "${ROM_DIR}"
"${BUILD_DIR}/vf2i960" compare-init "${ROM_DIR}"
"${BUILD_DIR}/vf2i960" compare-task-registry "${ROM_DIR}"
