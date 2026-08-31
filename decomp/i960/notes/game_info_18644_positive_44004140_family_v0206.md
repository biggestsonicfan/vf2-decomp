# v0206 0x44004140 family low-bit completion

Analogous to v0205 (0x24004140 = 26+29), extends the neighboring
positive-threshold high-pair family 0x44004140 (bits 6+14+26+30) from
single-mask admission to the full low-bit cube over bits 1,2,4.

## Measurement
* Same boundary `out/state8-positive.boundary.vf2snap`
* `validate_game_info_full_dispatch.py --mask --state 8` for each of the 8
  combinations `0x44004140 | low` where `low` in `{0,0x02,0x04,0x10,0x06,0x12,0x14,0x16}`
* Each validates 36/36 exact (3 distributions ×2 countdown ×2 mode6 ×3 thresholds):
  `0x44004140`, `0x44004142`, `0x44004144`, `0x44004146`, `0x44004150`, `0x44004152`, `0x44004154`, `0x44004156`
* Previously only `0x44004140` was admitted; the 7 low variants showed `instructions -4` DIFF and failed closed.

## Recovery
`src/recovered/hybrid.c:15696` expanded from 1 block to 4 blocks (2 masks each):
```
0x44004140 || 0x44004144
0x44004142 || 0x44004146
0x44004150 || 0x44004154
0x44004152 || 0x44004156
```
each `native_instructions += bilateral?4:2`, same `hybrid_set_compare_result` and stale-frame as the original 0x240/0x440 families.

Validation: all 8 masks 36/36 exact, `ctest -C Debug` 55/55 pass.
