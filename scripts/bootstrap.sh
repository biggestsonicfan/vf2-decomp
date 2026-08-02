#!/usr/bin/env sh
set -eu

cmake -S . -B build -DVF2_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure

printf '\nBuild complete.\n'
printf 'Next: build/vf2rom verify roms/vf2\n'
