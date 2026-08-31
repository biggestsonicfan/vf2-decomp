# v0216 positive 0x18140 (bits 6+15+16) low-bit completion

Extends the positive family `0x00018140` (bits 6+15+16) from single mask
to full low-bit cube over bits 1,2,4.

## Measurement
* `validate_game_info_full_dispatch.py --mask --state 8` for each of the 8
  masks `0x00018140 | low` where low in `{0,02,04,10,06,12,14,16}`
* Each 36/36 exact with `+9 bilateral / +4 unilateral` plus bit11 write
  at `fighter+0x1a4` (same as `0x10140`).
* Before, only base was admitted; 7 low variants were 0/36.

## Recovery
`src/recovered/hybrid.c` expanded from 1 block to 4 blocks:
`0x18140/44`, `0x18142/46`, `0x18150/54`, `0x18152/56` each `+9/+4` and same
stale-frame and memory write.

55/55 CTest pass.
