# Selector-3 phase-6 measured corridor (v0.0.26)

The controlled ROM corridor for selector 3 phase 6 (`0x0000b588`) was measured from the framed `0x0000a6c0` entry as **33,867 instructions / 6 calls / 7 returns**.

Observed call tree:

`0xa6c0 -> 0xacf8 -> 0xb588 -> 0x8ef0 -> 0x8f1c -> 0x8f1c -> 0x1344`

The recovered handler already models the tile clear, layer flags and both descriptor blits. The remaining mismatch was wrapper-level: the previous bridge fell through to the generic selector-3 cleanup (`260 / 1 / 2`), while the ROM returns early from `0xacf8`, leaving phase 7 and the phase mask `0x40`.

Commit `cc810acd6e81e5395eaefb43e0eff36573ea7069` adds the measured 33,867/6/7 corridor, preserves the early selector-3 return, reconstructs the second `0x8f1c` final destination from profile flags, restores its architecturally visible saved-`g9` spill, and derives the final `g2/g9` values from the descriptor column count instead of hardcoding them.

This note records the ROM measurement and implementation boundary; CI validates the source matrix, while full ROM snapshot comparison remains the acceptance criterion for declaring the phase closed.
