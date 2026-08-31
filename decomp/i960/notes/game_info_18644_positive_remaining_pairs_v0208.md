# v0208 remaining high pairs low-bit completion (29+30,29+31,30+31)

Completes the last three pairwise high-bit families for the
positive-threshold bit-6+14 state-8 corridor. Together with v0205
(26+29), v0206 (26+30) and v0207 (26+31), all six 2-high combinations
over bits 26,29,30,31 on the base `0x00004140` are now covered for the
low-bit cube.

## Measurement
* Same `validate_game_info_full_dispatch.py --mask --state 8` harness
* Each of the 24 masks (8 per family) 36/36 exact:
  * `0x60004140 | low` where `low` in `{0,02,04,10,06,12,14,16}` (29+30)
  * `0xa0004140 | low` (29+31)
  * `0xc0004140 | low` (30+31)
* Before, only the 3 base masks were admitted; the 21 low variants were `0/36 DIFF -4`.

## Recovery
`src/recovered/hybrid.c` expanded:
* `0x60004140` : 1 → 4 blocks (8 masks)
* `0xa0004140` : 1 → 4 blocks (8 masks)
* `0xc0004140` : 1 → 4 blocks (8 masks)
each `+4 bilateral/+2 unilateral`, same compare and stale-frame as
previous families.

All six high pairs ×8 low masks = 48 masks for the positive
bit-6 high-pair corridor are now 36/36 exact.

55/55 CTest pass.
