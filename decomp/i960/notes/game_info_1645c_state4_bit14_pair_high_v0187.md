# v0187: state-4 bit14 pair-high continuation

This extends the v0186 state-4 pair-high recovery at entry `0x0001645c` to the next two low families: bit14 alone (`0x4000`) and bit14+bit6 (`0x4040`), combined with exactly two high bits among 21, 26, 29, 30, and 31.

ROM measurement shows the same poststate as the neutral v0186 family: instruction accounting and mutable RAM already match, compare finishes `NONE`, and `fa_game_info` continues at scheduler epilogue entry `0x00010dd0` for nonnegative thresholds. No instruction or memory correction is added.

## Verification

All 10 high-bit pairs x 2 low families x 3 fighter distributions x 2 countdown values x 2 mode-bit-6 values x thresholds 0/1/2 pass: **720/720 exact ROM-backed snapshots**.

Threshold `-1` remains outside the new predicate and passes as a negative control across the same pairs/families/distributions/countdown/mode matrix: **240/240 exact snapshots**.

The local build succeeds and CTest passes **23/23**.
