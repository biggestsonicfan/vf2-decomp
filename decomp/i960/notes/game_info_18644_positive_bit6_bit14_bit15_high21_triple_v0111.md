# `fa_game_info` positive state-8 bit-14/15 high-bit triple 21+26+29

The mask `0x2420c140` (state bit 8, bit 6, bits 14 and 15, and high bits
21+26+29) is now admitted through an explicit `0x164c4` return-corridor
accounting rule.

Its complete 12-case matrix matched reference snapshots and instruction,
call, and return counters for fighter-0-only, fighter-1-only, and bilateral
flags with countdown `0/1` and mode bit 6 `0/1`. The unilateral corrections
are asymmetric by fighter order; the bilateral cases independently match.

The neighboring unadmitted mask `0x4420c140` (21+26+30) remained 3/12 exact.

Reproduce with `validate_game_info_state4.py --state 8`, extra bits 14, 15,
21, 26 and 29, `--mask 248`, `--threshold 0`, and the calibrated
`out/state8-positive.boundary.vf2snap` base.
