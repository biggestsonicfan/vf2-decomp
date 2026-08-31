# v0207 0x84004140 family low-bit completion

Extends the positive-threshold high-pair family 0x84004140 (bits
6+14+26+31) from single-mask to full low-bit cube over bits 1,2,4,
analogous to v0205/0206.

## Measurement
* `validate_game_info_full_dispatch.py --mask --state 8` for each of the 8
  combinations `0x84004140 | low` where `low` in `{0,0x02,0x04,0x10,0x06,0x12,0x14,0x16}`
* Each 36/36 exact (3 distributions ×2 countdown ×2 mode6 ×3 thresholds 0..2):
  `0x84004140`, `0x84004142`, `0x84004144`, `0x84004146`, `0x84004150`, `0x84004152`, `0x84004154`, `0x84004156`
* Previously only base was admitted; low variants were 0/36 `DIFF -4`.

## Recovery
`src/recovered/hybrid.c:15840` expanded from 1 block to 4 blocks:
```
0x84004140 || 0x84004144
0x84004142 || 0x84004146
0x84004150 || 0x84004154
0x84004152 || 0x84004156
```
each `+4 bilateral/+2 unilateral`, same compare and stale-frame as
0x240/0x440 families.

55/55 CTest pass.
