# v0213 positive 0x8140 (bit15+14+6) low-bit completion

Extends the positive bit-6+14+15 family `0x00008140` from single mask
to full low-bit cube over bits 1,2,4.

## Measurement
* `validate_game_info_full_dispatch.py --mask --state 8` for each of the 8
  masks `0x00008140 | low` where low in `{0,02,04,10,06,12,14,16}`
* Each 36/36 exact with distinct accounting `-5 bilateral / -3 unilateral`
  (vs `+4/+2` for the `0x4140` high family).
* Before, only base was admitted; 7 low variants were 0/36.

## Recovery
`src/recovered/hybrid.c` expanded from 1 block to 4 blocks:
`0x8140/44`, `0x8142/46`, `0x8150/54`, `0x8152/56` each `-5/-3` and same stale-frame.

55/55 CTest pass.
