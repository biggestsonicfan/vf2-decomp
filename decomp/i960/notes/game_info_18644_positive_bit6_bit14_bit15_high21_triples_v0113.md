# `fa_game_info` positive state-8 bit-14/15 high-bit triples

The remaining four bit-14 + bit-15 extensions with state bit 8, bit 6 and
high bit 21 are now admitted through isolated `0x164c4` return-corridor
accounting rules:

- `0x8420c140` (`21+26+31`)
- `0x6020c140` (`21+29+30`)
- `0xa020c140` (`21+29+31`)
- `0xc020c140` (`21+30+31`)

Each complete 12-case matrix matched reference snapshots and instruction,
call, and return counters for fighter-0-only, fighter-1-only, and bilateral
flags with countdown `0/1` and mode bit 6 `0/1`. Together with the previously
measured `0x2420c140` and `0x4420c140` cases, this completes all six pairs of
high bits chosen from 26, 29, 30 and 31 with bit 21 for this family.

Reproduce with `validate_game_info_state4.py --state 8`, the corresponding
extra bits from each mask, `--mask 248`, `--threshold 0`, and the calibrated
`out/state8-positive.boundary.vf2snap` base.
