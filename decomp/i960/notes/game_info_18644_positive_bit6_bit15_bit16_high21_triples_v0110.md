# `fa_game_info` positive state-8 bit-15/16 high-bit-21 triples

The existing state-8 bit-15 + bit-16 corridor was measured for all six triples
formed by bit 21 and two of high bits 26, 29, 30 and 31:

| mask | high-bit composition | result |
| --- | --- | --- |
| `0x24218140` | `21+26+29` | 12/12 exact |
| `0x44218140` | `21+26+30` | 12/12 exact |
| `0x84218140` | `21+26+31` | 12/12 exact |
| `0x60218140` | `21+29+30` | 12/12 exact |
| `0xa0218140` | `21+29+31` | 12/12 exact |
| `0xc0218140` | `21+30+31` | 12/12 exact |

Each matrix used state 8, threshold `0`, fighter-0-only, fighter-1-only and
bilateral distributions, countdown `0/1`, and mode bit 6 `0/1`. Every case
matched reference snapshots and instruction/call/return counters. No runtime
change was needed; these measurements document existing exact coverage.

Reproduce with `validate_game_info_state4.py --state 8`, extra bits 15, 16,
21 and the selected two high bits, `--mask 248`, `--threshold 0`, and the
calibrated `out/state8-positive.boundary.vf2snap` base.
