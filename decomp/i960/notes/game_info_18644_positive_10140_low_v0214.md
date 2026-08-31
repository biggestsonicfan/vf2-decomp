# v0214 positive 0x10140 (bit16+14+6) low-bit completion

Extends the positive bit-6+14+16 family `0x00010140` from single mask
to full low-bit cube over bits 1,2,4.

## Measurement
* `validate_game_info_full_dispatch.py --mask --state 8` for each of the 8
  masks `0x00010140 | low` where low in `{0,02,04,10,06,12,14,16}`
* Each 36/36 exact with distinct accounting `+8 bilateral / +4 unilateral`
  plus the `bit11` write at `fighter+0x1a4` (preserved for low variants).
* Before, only base was admitted; 7 low variants were 0/36.

## Recovery
`src/recovered/hybrid.c` expanded from 1 block to 4 blocks:
`0x10140/44`, `0x10142/46`, `0x10150/54`, `0x10152/56` each `+8/+4` and same
stale-frame and memory write.

55/55 CTest pass.
