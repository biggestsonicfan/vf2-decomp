# v0218 compact low cubes for 0xC140/0x1C140

Compacts four explicit 4-mask blocks into one predicate.

## Measurement
* Bases `0xC140` (bits 6+14+15) and `0x1C140` (plus bit16) each with low cube over bits 1,2,4 => 16 masks `0x base | low`
* `validate_game_info_full_dispatch.py --mask --state 8` for all 16 masks `36/36 exact` with `+5 bilateral / +2 unilateral`

## Recovery
`src/recovered/hybrid.c` replaced 4 `combined == A||B||C||D` blocks with one
`(combined & ~0x10016)==0xC140 && (combined & 0xC140)==0xC140` predicate
(masking low bits and bit16). Preserves stale-frame and LESS/EQUAL.

55/55 CTest pass.
