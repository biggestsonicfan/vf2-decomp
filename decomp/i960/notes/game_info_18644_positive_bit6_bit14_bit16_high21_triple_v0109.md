# `fa_game_info` positive state-8 bit-14/16 high-bit triple 21+29+31

The measured mask `0xa0214140` (state bit 8, bit 6, bits 14 and 16, and high
bits 21+29+31) now uses the explicit `0x164c4` return-corridor correction.
All 12 controlled cases matched reference snapshots and instruction,
call, and return counters across fighter-0-only, fighter-1-only, and bilateral
flags, countdown `0/1`, and mode bit 6 `0/1`.

This completes the six measured bit-14 + bit-16 triples formed by bit 21 and
two of high bits 26, 29, 30, and 31. Other positive high-bit compositions are
not admitted by this change.

Reproduce with `validate_game_info_state4.py --state 8`, extra bits 14, 16,
21, 29 and 31, `--mask 248`, `--threshold 0`, and the calibrated
`out/state8-positive.boundary.vf2snap` base.
