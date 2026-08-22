# `fa_game_info` `0x18644`: positive state-8 bit-14/bit-16 and bit-15/bit-16 all-high masks

The all-four high-bit composition from bits 26, 29, 30 and 31 was measured for
each adjacent fighter-bit composition:

| Fighter-bit composition | Mask |
| --- | ---: |
| 14 + 16 | `0xe4014140` |
| 15 + 16 | `0xe4018140` |

Each focused matrix covered all three physical distributions, countdown 0/1
and mode-byte bit 6 clear/set: 12 cases per composition. Every case matched
the ROM at the scheduler boundary, including CPU state, condition state,
mutable memory and instruction/call/return counters.

The native recovery admits exactly these two masks. The bit-14 + bit-16 path
reuses its measured triple corrections: `-1`/`+4` for asymmetric mode-bit-6
joins, `-1`/`-6` for bilateral no-countdown/mode joins, and `-1` for countdown
bilateral cases. The bit-15 + bit-16 path reuses `-15`/`-10` for countdown
first-order joins, `-2`/`-2` for mode-bit-6 first-order joins, and `-1`, `-13`
and `-26` for bilateral no-countdown, mode-bit-6 and countdown cases. Other
larger or high-bit-21 combinations remain explicit unsupported boundaries.

Reproduction uses the same validator with the relevant fighter-bit dimensions,
all four high-bit dimensions and `--mask 2040`.

Expected result for each listed mask: `summary: 12/12 exact`.
