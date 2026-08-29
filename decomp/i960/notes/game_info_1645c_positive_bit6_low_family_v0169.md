# v0169: recover positive state-8 bit-6 low-bit family

Entry `0x0001645c` with `fighter0_state==8 && fighter1_state==8`,
`shared_fighter_threshold >=0`, and `measured_matrix_distribution` (bilateral,
unilateral, zero distributions covered).

Previously the dispatcher admitted the high-bit families
(`0x24200140` etc) and the isolated `0x140` was still unadmitted for full
dispatch (12/12 DIFF, +3/+8 instruction deltas, arch `ac=0x3f001000 cr0`
vs `ac=0x3f001002 cr2` for cd0 and `ac=0x3f001004 cr1` for cd1).

Newly admitted masks (each `combined_state8_flags`):

- `0x00000140` (bit6+bit8)
- `0x00000150` (bit4+6+8)
- `0x00000142` (bit1+6+8)
- `0x00000144` (bit2+6+8)
- `0x00000146` (bit1+2+6+8)
- `0x00000152` (bit1+4+6+8)
- `0x00000154` (bit2+4+6+8)
- `0x00000156` (bit1+2+4+6+8)

Each is the complete low-bit subset family with bit6+bit8 and no high bits
(21/26/29/30/31 clear). The ROM dispatcher for these masks was measured
with `decomp/i960/tools/validate_game_info_full_dispatch.py` across
3 distributions × 2 countdown × 2 mode6 × thresholds 0..2 (36 fixtures per
mask, 288 total). Previously they failed with instruction deltas +3/+8/+11/+6
and condition `NONE` vs `EQUAL`/`LESS`.

Recovered semantics in `src/recovered/hybrid.c` (dispatcher):

- `0x140`/`0x150` share the existing high-3-4 family accounting:
  `bilateral?6:3` plus `!countdown && mode6 ? (fighter0_only?5:1) : 0`,
  stale `r3=0x41000000 r4=0x07800f0f r7=0x41000000 r15=1` for non-f0-only,
  condition `EQUAL` if countdown 0 else `LESS`.

- `0x142`/`0x144`/`0x146`/`0x152`/`0x154`/`0x156` use a distinct
  countdown-only rule: `countdown? (bilateral?6:3) : (bilateral?11:8)`,
  same stale frame and same `EQUAL`/`LESS` condition. This was inferred
  from the measured +8/+3/+11/+6 matrix and verified by streaming the
  reference through `native-resume` vs `resume-trace`.

Validation:

- `python decomp/i960/tools/validate_game_info_full_dispatch.py build/Debug/vf2i960.exe roms/vf2 --mask 0x140 --thresholds 0,1,2  => 36/36 exact`
- same for `0x150`, `0x142`, `0x144`, `0x146`, `0x152`, `0x154`, `0x156`
- `ctest -R "vf2_native_second|vf2_native_fifth|vf2_native_sixth|vf2_native_seventh"` still MATCH (second 1270824/0, fifth 836 blocks, etc)
- `ctest -R "vf2_tests|vf2_player_planar_rotation"` still PASS

Remaining: high-bit cross-family compositions `0x4140`/`0x8140`/etc remain
fail-closed (-2/+3 deltas) and are not claimed.
