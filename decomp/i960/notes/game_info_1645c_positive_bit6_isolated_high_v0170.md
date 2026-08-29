# v0170: recover positive state-8 bit-6 isolated high family

Entry `0x0001645c` with `fighter0_state==8 && fighter1_state==8`,
`measured_matrix_distribution` (bilateral, unilateral, zero), and
`shared_fighter_threshold >=0` (validated for thresholds 0..2).

Previously the dispatcher for `bit6+bit8` plus a single high bit
14/15/16 or their pairwise/triple combos was fail-closed for full
dispatch (36-case matrices all DIFF with instruction deltas -2/-4,
+3/+5, -4/-8, -2/-5, -4/-9 etc and `ac=0x3f001000 cr0` vs
`ac=0x3f001002 cr2` for cd0 and `ac=0x3f001004 cr1` for cd1, plus
stale frame at `local_frames[depth+1]` with `41000000 07800f0f ...`).

Newly admitted masks (each `combined = fighter0|fighter1`):

- `0x00004140` (bit14 + bit6 + bit8)
- `0x00008140` (bit15 + bit6 + bit8)
- `0x00010140` (bit16 + bit6 + bit8)
- `0x0000c140` (bit14+15 + bit6 + bit8)
- `0x00014140` (bit14+16 + bit6 + bit8)
- `0x00018140` (bit15+16 + bit6 + bit8)
- `0x0001c140` (bit14+15+16 + bit6 + bit8)

Each now matches the full 36-case dispatcher matrix
(3 distributions ×2 countdown ×2 mode6 × thresholds 0..2)
via measured dispatcher accounting, countdown-derived condition
and stale-frame postconditions, while the inner `0x18644` child
remains on the ROM fallback (conditional path).

Recovered semantics in `src/recovered/hybrid.c` (dispatcher,
direct positive-state8 gate `fighter0==8 && fighter1==8 &&
measured_matrix_distribution && threshold>=0`):

- `0x4140`/`0x14140`: `bilateral?4:2` (native short) + `LESS/EQUAL`
- `0xc140`/`0x1c140`: `bilateral?5:2` + `LESS/EQUAL`
- `0x8140`: `bilateral?5:3` excess (native long, subtract) + `LESS/EQUAL`
- `0x10140`: `bilateral?8:4` + `LESS/EQUAL` plus `fighter+0x1a4` bit11 set
- `0x18140`: `bilateral?9:4` + `LESS/EQUAL` plus `fighter+0x1a4` bit11 set

Stale for all: `r3=0x41000000 r4=0x07800f0f r7=0x41000000`;
`r8=r12=0x07800f0f r13=0x3f6b871d r14=0 r15=1` for bilateral or
f1-only, and `r14=8 r15=0 r8=r12=0` for f0-only, matching the
reference `local_frames[depth+1]` at `0x18584`.

Validation:

- `validate_game_info_full_dispatch.py --mask 0x4140/0x8140/0x10140/0xc140/0x14140/0x18140/0x1c140 --thresholds 0,1,2` each `36/36 exact`
- also `0x140` low family still `36/36`
- `ctest -R "vf2_native_second|fifth|sixth|seventh"` still MATCH

Remaining: cross-family high-bit-21 compositions `0x204140` etc
remain fail-closed and are the next frontier (they show asymmetric
f0-only vs f1-only and memory diff beyond dispatcher).
