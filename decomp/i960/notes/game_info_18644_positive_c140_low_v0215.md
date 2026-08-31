# v0215 positive 0xC140/0x1C140 low-bit completion (6+14+15 [+16])

Extends the positive families `0x0000C140` (bits 6+14+15) and
`0x0001C140` (bits 6+14+15+16) from 2 masks (paired) to full low-bit
cubes over bits 1,2,4.

Each high variant contributes 8 low masks: `0x...C140,42,44,46,50,52,54,56`
→ 16 masks total, 14 new low variants.

## Measurement
* `validate_game_info_full_dispatch.py --mask --state 8` for each of the
  16 masks, each 36/36 exact with `+5 bilateral / +2 unilateral`.
* Before, only the 2 bases were admitted; 14 low variants were 0/36.

## Recovery
`src/recovered/hybrid.c` expanded from 1 block (2 masks) to 4 blocks
(4 masks each, mixing both high variants per block):
`C140/44 +1C140/44`, `C142/46+1C142/46`, `C150/54+1C150/54`, `C152/56+1C152/56`
each `+5/+2` and same stale-frame.

55/55 CTest pass.
