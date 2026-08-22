# `fa_game_info` positive state-8 bit-14/16 high-bit triple 21+29+30

The measured mask `0x60214140` (state bit 8, bit 6, bits 14 and 16, and high
bits 21+29+30) now uses the recovered `0x164c4` return-corridor accounting.
Its standard 12-case matrix—fighter-0-only, fighter-1-only and bilateral,
countdown `0/1`, mode bit 6 `0/1`—matched reference snapshots and all
instruction/call/return counters.

The neighboring unadmitted mask `0xa0214140` (21+29+31) remained 3/12 exact.
No other high-bit triple was admitted.

Reproduce with `validate_game_info_state4.py --state 8`, extra bits 14, 16,
21, 29 and 30, `--mask 248`, `--threshold 0`, and the calibrated
`out/state8-positive.boundary.vf2snap` base.
