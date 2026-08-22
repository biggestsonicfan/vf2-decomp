# `fa_game_info` `0x18644`: positive state-8 bit-15/bit-16 high-bit extensions

The measured positive-threshold fighter fields combining state bit 8, fighter
bit 6, fighter bit 15 or 16, and one of high bits 26, 29, 30 or 31 were
recovered at the `0x18644` child:

| Fighter bit | High bit | Mask |
| ---: | ---: | ---: |
| 15 | 26 | `0x4008140` |
| 15 | 29 | `0x20008140` |
| 15 | 30 | `0x40008140` |
| 15 | 31 | `0x80008140` |
| 16 | 26 | `0x4010140` |
| 16 | 29 | `0x20010140` |
| 16 | 30 | `0x40010140` |
| 16 | 31 | `0x80010140` |

Each focused matrix covered all three physical distributions, countdown 0/1
and mode-byte bit 6 clear/set: 12 cases per mask, 96 cases total. Every case
matched the ROM at the scheduler boundary, including CPU state, condition
state, mutable memory and instruction/call/return counters.

The native recovery admits only one of these four high bits with bit 6 and
bit 15 or 16. For bit 15, the measured first-order, second-order and bilateral
joins use the validated accounting rule: the countdown reductions are 15 and
10 for the two first-order distributions, and the bilateral reductions are 1,
11 and 26 for the measured mode/countdown combinations. For bit 16, the
measured correction is bilateral only: reductions are 1 for countdown with
mode bit 6 clear, 10 for mode bit 6 set without countdown, and 1 for the
countdown cases. Multiple high bits in either cross-family composition remain
explicit unsupported boundaries.

Reproduction for each fighter bit and high bit:

```bash
for fighter_bit in 15 16; do
  for high_bit in 26 29 30 31; do
    python3 decomp/i960/tools/validate_game_info_state4.py \
      ./build/vf2i960 /path/to/vf2-roms \
      --state 8 --include-bit8 --extra-bit 6 \
      --extra-bit "$fighter_bit" --extra-bit "$high_bit" \
      --mask 120 --threshold 0 --base out/state8-positive.boundary.vf2snap
  done
done
```

Expected result: `summary: 12/12 exact` for every iteration.
