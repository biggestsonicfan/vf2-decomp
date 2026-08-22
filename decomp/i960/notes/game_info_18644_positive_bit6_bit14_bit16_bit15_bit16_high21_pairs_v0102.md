# `fa_game_info` `0x18644`: positive state-8 bit-14/bit-16 and bit-15/bit-16 high-bit-21 pairs

The four pairs of high bit 21 with bits 26, 29, 30 and 31 were measured for
each adjacent fighter-bit composition:

| Fighter-bit composition | High-bit pair | Mask |
| --- | --- | ---: |
| 14 + 16 | 21 + 26 | `0x04214140` |
| 14 + 16 | 21 + 29 | `0x20214140` |
| 14 + 16 | 21 + 30 | `0x40214140` |
| 14 + 16 | 21 + 31 | `0x80214140` |
| 15 + 16 | 21 + 26 | `0x04218140` |
| 15 + 16 | 21 + 29 | `0x20218140` |
| 15 + 16 | 21 + 30 | `0x40218140` |
| 15 + 16 | 21 + 31 | `0x80218140` |

Each focused matrix covered all three physical distributions, countdown 0/1
and mode-byte bit 6 clear/set: 12 cases per composition/pair. Every case
matched the ROM at the scheduler boundary, including CPU state, condition
state, mutable memory and instruction/call/return counters.

The native recovery admits exactly these eight pair masks. Bit-14 + bit-16
uses the measured mode-bit-6 corrections `-1`/`+4` for the two asymmetric
orders. Bit-15 + bit-16 uses `-15`/`-10` for countdown and `-2`/`-2` for
mode-bit-6 in the two asymmetric orders. Other high-bit-21 combinations
remain explicit unsupported boundaries.

Reproduction uses the same validator with `--extra-bit 21` and one of
`--extra-bit 26`, `29`, `30` or `31`, `--mask 384`, and the relevant
bit-14/16 or bit-15/16 dimensions.

Expected result for each listed pair: `summary: 12/12 exact`.
