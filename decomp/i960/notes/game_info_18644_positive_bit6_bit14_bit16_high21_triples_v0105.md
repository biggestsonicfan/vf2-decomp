# `fa_game_info` positive state-8 bit-14/16 high-bit triples

The existing `0x164c4` recovery corridor was extended for two measured
bit-14 + bit-16 triples:

| mask | high-bit composition | result |
| --- | --- | --- |
| `0x24214140` | `21+26+29` | 12/12 exact |
| `0x44214140` | `21+26+30` | 12/12 exact |

Both matrices used state 8, threshold `0`, fighter-0-only, fighter-1-only and
bilateral distributions, countdown `0/1`, and mode bit 6 `0/1`. Snapshots and
instruction/call/return counters matched the reference in every case.

The unadmitted neighboring mask `0x60214140` (`21+29+30`) remained 3/12 exact,
providing a fail-closed control. No other high-bit triple was admitted.

Reproduce either exact matrix with `validate_game_info_state4.py`, using
`--state 8`, `--extra-bit 14 --extra-bit 16 --extra-bit 21`, the selected two
high-bit `--extra-bit` arguments, `--mask 248`, `--threshold 0`, and the
calibrated `out/state8-positive.boundary.vf2snap` base.
