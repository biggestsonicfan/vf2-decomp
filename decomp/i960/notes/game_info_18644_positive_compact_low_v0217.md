# v0217 compact low cubes for 0x8140/0x10140/0x18140

Compacts twelve explicit pair blocks into three predicates over low bits 1,2,4.

## Measurement
* Each base `0x8140`, `0x10140`, `0x18140` with `low = bits 1,2,4` => 8 masks each
  `0x base | low` where low in `{0,02,04,06,10,12,14,16}`
* `validate_game_info_full_dispatch.py --mask --state 8` for all 24 masks
  `36/36 exact` with existing accounting:
  - `0x8140`: `-5 bilateral / -3 unilateral`
  - `0x10140`: `+8 / +4` plus `fighter+0x1a4` bit11 write for any low variant
  - `0x18140`: `+9 / +4` plus same bit11 write
* Before compact, `0x10144` and `0x18144` were `0/36` due to narrow
  `fighter_flags == 0x10140` check that missed low-variant solo distributions;
  generic `fighter_flags == combined` fixes them to `36/36`.

## Recovery
`src/recovered/hybrid.c` replaced 12 `combined == A || combined == B` blocks
with 3 ` (combined & ~0x16) == base` predicates, preserving stale-frame
`r3=0x41000000 r4/r7=0x07800f0f` etc and `LESS/EQUAL` compare postcondition.

55/55 CTest pass, 24/24 low-cube masks 36/36 exact.
