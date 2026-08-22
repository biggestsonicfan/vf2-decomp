# `fa_game_info` `0x18644`: positive state-8 bit-14/bit-16 and bit-15/bit-16 high triples

The four high-bit triples from 26, 29, 30 and 31 were measured for each of the
two adjacent fighter-bit compositions:

| Fighter-bit composition | High-bit triple | Mask |
| --- | --- | ---: |
| 14 + 16 | 26 + 29 + 30 | `0x64014140` |
| 14 + 16 | 26 + 29 + 31 | `0xa4014140` |
| 14 + 16 | 26 + 30 + 31 | `0xc4014140` |
| 14 + 16 | 29 + 30 + 31 | `0xe0014140` |
| 15 + 16 | 26 + 29 + 30 | `0x64018140` |
| 15 + 16 | 26 + 29 + 31 | `0xa4018140` |
| 15 + 16 | 26 + 30 + 31 | `0xc4018140` |
| 15 + 16 | 29 + 30 + 31 | `0xe0018140` |

Each focused matrix covered all three physical distributions, countdown 0/1
and mode-byte bit 6 clear/set: 12 cases per composition/triple. Every case
matched the ROM at the scheduler boundary, including CPU state, condition
state, mutable memory and instruction/call/return counters.

The native recovery admits exactly these eight triple masks. The bit-14 +
bit-16 triples reuse the measured pair corrections: `-1`/`+4` for asymmetric
mode-bit-6 joins, `-1`/`-6` for bilateral no-countdown/mode joins, and `-1` for
countdown bilateral cases. The bit-15 + bit-16 triples reuse their measured
pair corrections: `-15`/`-10` for countdown first-order joins, `-2`/`-2` for
mode-bit-6 first-order joins, and `-1`, `-13` and `-26` for bilateral
no-countdown, mode-bit-6 and countdown cases. Other triple and larger
high-bit combinations remain explicit unsupported boundaries.

Reproduction uses the same validator with the relevant four high bits listed
as three `--extra-bit` dimensions and `--mask 1016`; run it once for each
fighter-bit composition and high-bit triple listed above.

Expected result for each listed triple: `summary: 12/12 exact`.
