# `fa_game_info` `0x1645c`: full state-8 bit-14/bit-16/high-bit-26 composition

The full dispatcher was measured for field mask `0x04214000`, corresponding
to the already-proven child composition `0x04214140` after the state-8 child
selector/low-field bits are removed. The native child admission was checked
for all three physical fighter-record distributions, countdown 0/1, mode-byte
bit 6 clear/set, and thresholds 0, 1 and 2.

All 36 full-task cases matched the ROM at `0x00010dcc`, including CPU state,
condition state, mutable memory and instruction/call/return counters. The
dispatcher accounting is the measured bit-14/bit-16 high-bit rule: unilateral
mode/countdown joins are `+2*mode6 + countdown - mode6*countdown` for fighter0
and `-4*mode6 - 4*countdown + 4*mode6*countdown` for fighter1; the bilateral
correction is `+1 - 2*mode6 - 3*countdown + 3*mode6*countdown`.

Admission remains limited to the exact field mask and thresholds 0 through 2;
other high-bit compositions and thresholds remain ROM-backed boundaries.
