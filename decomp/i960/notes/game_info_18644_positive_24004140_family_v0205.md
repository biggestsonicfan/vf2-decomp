# v0205 0x24004140 family low-bit completion

Extends `0x24004140` (state8 + bits 6+8+14+26+29) from single mask
(v0161) and `0x24004144` (v0204, bit2) to the full low-bit cube
`bits 1,2,4` (masks `0x02,0x04,0x10`).

## Measurement
* Boundary `out/state8-positive.boundary.vf2snap` (0x00510980)
* `validate_game_info_full_dispatch.py --mask` for each of the 8
  combinations: `0x24004140 | low` where `low` in
  `{0,0x02,0x04,0x10,0x06,0x12,0x14,0x16}` (all subsets of bits 1,2,4)
* Each of the 8 masks validated via 36-case full dispatch
  (3 distributions ×2 countdown ×2 mode6 ×3 thresholds 0,1,2):
  * `0x24004140` 36/36 exact (v0161)
  * `0x24004144` 36/36 exact (v0204, bit2)
  * `0x24004142`/`0x24004146` 36/36 exact (bit1, bit1+2)
  * `0x24004150`/`0x24004154` 36/36 exact (bit4, bit2+4)
  * `0x24004152`/`0x24004156` 36/36 exact (bit1+4, bit1+2+4)

The `trace_case` child-only run showed varying `run_instructions`
(189 vs 211 vs 199 vs 221) for different low subsets, but the full
dispatch (which includes scheduler + dispatcher accounting) is
uniform: each of the 8 masks uses `bilateral +4 / unilateral +2`
plus the same stale-frame postconditions
`r3=0x41000000 r4=0x07800f0f r7=0x41000000` etc., as in
`hybrid.c:15552`. The variation in child-only counts is absorbed by
the dispatcher vs child split.

## Recovery
`src/recovered/hybrid.c:15552` now has 4 blocks (2 masks each) covering
all 8:
```c
0x24004140 || 0x24004144
0x24004142 || 0x24004146
0x24004150 || 0x24004154
0x24004152 || 0x24004156
```
each `native_instructions += bilateral?4:2`, same `hybrid_set_compare_result`
and stale-frame.

Previously only `0x24004140` (and `0x24004144` after v0204) were admitted;
the other 6 were `VF2_ERROR_UNSUPPORTED`.

## Validation
* Each of the 8 masks: `validate_game_info_full_dispatch.py --mask 0x... --state 8`
  reports `36/36 exact` (snapshot, arch, counters)
* `ctest -C Debug` 55/55 pass
* No regression for other high-bit families
