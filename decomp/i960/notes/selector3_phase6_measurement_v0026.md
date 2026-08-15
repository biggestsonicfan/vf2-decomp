# Selector-3 phase-6 measured corridor (v0.0.26)

The controlled ROM corridor for selector 3 phase 6 (`0x0000b588`) was measured from the framed `0x0000a6c0` entry as **33,867 instructions / 6 calls / 7 returns**.

Observed call tree:

`0xa6c0 -> 0xacf8 -> 0xb588 -> 0x8ef0 -> 0x8f1c -> 0x8f1c -> 0x1344`

The recovered handler models the tile clear, layer flags and both descriptor blits. The previous bridge fell through to the generic selector-3 cleanup (`260 / 1 / 2`), while the ROM returns early from `0xacf8`, leaving phase 7 and the phase mask `0x40`.

Commit `cc810acd6e81e5395eaefb43e0eff36573ea7069` adds the measured 33,867/6/7 corridor, preserves the early selector-3 return, reconstructs the second `0x8f1c` final destination from profile flags, restores its architecturally visible saved-`g9` spill, and derives the final `g2/g9` values from the descriptor column count instead of hardcoding them.

Commit `a919a64fc1e98b4466991b21e4dc9e3af4ec3ef0` completes the observed `0x8f1c` register poststate. For real descriptors it derives `g4` from the signed addend, `g5=0`, `g6` from the final signed sample plus addend, and `g7` from the descriptor word mode; synthetic reduced fixtures without descriptor payload retain their prior compatibility path instead of fabricating descriptor contents.

Full ROM snapshot comparison remains the acceptance criterion for declaring the phase closed.
