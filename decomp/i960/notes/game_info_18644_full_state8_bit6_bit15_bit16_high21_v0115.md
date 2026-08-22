# `fa_game_info` `0x1645c`: full state-8 bit-6/bit-15/bit-16/bit-21 composition

The full dispatcher was measured for field mask `0x00218000`, corresponding
to the already-proven child composition `0x218140` after the state-8 child
selector/low-field bits are removed. The native child admission was checked
for all three physical fighter-record distributions, countdown 0/1, mode-byte
bit 6 clear/set, and thresholds 0, 1 and 2.

All 36 full-task cases matched the ROM at `0x00010dcc`, including CPU state,
condition state, mutable memory and instruction/call/return counters. The
unilateral distributions use the native-child baseline; the bilateral
distribution requires a fixed `+2` dispatcher-instruction correction.

Admission remains limited to the exact field mask and thresholds 0 through 2;
other positive compositions and thresholds remain ROM-backed boundaries.
