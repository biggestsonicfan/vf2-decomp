# `fa_game_info` `0x18644`: positive state-8 bit-14/bit-16 and bit-15/bit-16 high pairs

The six pairwise high-bit combinations from 26, 29, 30 and 31 were measured
for each of the two adjacent fighter-bit compositions:

| Fighter-bit composition | High-bit pair | Mask |
| --- | --- | ---: |
| 14 + 16 | 26 + 29 | `0x24014140` |
| 14 + 16 | 26 + 30 | `0x44014140` |
| 14 + 16 | 26 + 31 | `0x84014140` |
| 14 + 16 | 29 + 30 | `0x60014140` |
| 14 + 16 | 29 + 31 | `0xa0014140` |
| 14 + 16 | 30 + 31 | `0xc0014140` |
| 15 + 16 | 26 + 29 | `0x24018140` |
| 15 + 16 | 26 + 30 | `0x44018140` |
| 15 + 16 | 26 + 31 | `0x84018140` |
| 15 + 16 | 29 + 30 | `0x60018140` |
| 15 + 16 | 29 + 31 | `0xa0018140` |
| 15 + 16 | 30 + 31 | `0xc0018140` |

Each focused matrix covered all three physical distributions, countdown 0/1
and mode-byte bit 6 clear/set: 12 cases per composition/pair. Every case
matched the ROM at the scheduler boundary, including CPU state, condition
state, mutable memory and instruction/call/return counters.

The native recovery admits exactly these twelve pair masks. The bit-14 +
bit-16 pairs reuse the measured single-high corrections: `-1`/`+4` for the
asymmetric mode-bit-6 joins, `-1`/`-6` for bilateral no-countdown/mode joins,
and `-1` for countdown bilateral cases. The bit-15 + bit-16 pairs reuse their
corresponding measured corrections: `-15`/`-10` for countdown first-order
joins, `-2`/`-2` for mode-bit-6 first-order joins, and `-1`, `-13` and `-26`
for bilateral no-countdown, mode-bit-6 and countdown cases. Other pair and
larger high-bit combinations remain explicit unsupported boundaries.

Reproduction uses the same validator with `--extra-bit 6 --extra-bit 14
--extra-bit 16` for the first six rows and `--extra-bit 6 --extra-bit 15
--extra-bit 16` for the last six rows. Select the two high bits as the only
extra dimensions with `--mask 384`.

Expected result for each listed pair: `summary: 12/12 exact`.
