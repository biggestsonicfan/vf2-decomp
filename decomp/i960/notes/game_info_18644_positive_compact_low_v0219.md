# v0219 compact low cubes for 0x4140/0x14140

Compacts eight explicit pair blocks into one predicate.

## Measurement
* Bases `0x4140` (bits 6+14) and `0x14140` (plus bit16) each with low cube over bits 1,2,4 => 16 masks
* `validate_game_info_full_dispatch.py --mask --state 8` for all 16 masks `36/36 exact` with `+4 bilateral / +2 unilateral`

## Recovery
`src/recovered/hybrid.c` replaced 8 `combined == A||B` blocks with one
`(combined & ~0x10016)==0x4140 && (combined & 0x4140)==0x4140` predicate.

55/55 CTest pass.
