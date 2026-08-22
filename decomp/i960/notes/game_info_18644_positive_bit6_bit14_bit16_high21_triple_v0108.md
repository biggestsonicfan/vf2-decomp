# `fa_game_info` positive state-8 bit-14/16 high-bit triple 21+30+31

The measured mask `0xc0214140` (state bit 8, bit 6, bits 14 and 16, and high
bits 21+30+31) now uses the explicit `0x164c4` return-corridor correction.
The complete 12-case matrix matched reference snapshots and instruction,
call, and return counters for fighter-0-only, fighter-1-only, and bilateral
flags with countdown `0/1` and mode bit 6 `0/1`.

The unadmitted neighboring mask `0xa0214140` (21+29+31) remained 3/12 exact.
No other high-bit triple was admitted.

Reproduce with `validate_game_info_state4.py --state 8`, extra bits 14, 16,
21, 30 and 31, `--mask 248`, `--threshold 0`, and the calibrated
`out/state8-positive.boundary.vf2snap` base.
