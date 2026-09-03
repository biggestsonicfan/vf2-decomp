# game_info 0x18644 positive high-family for base 0x140 low variants — v0242-v0244

## Scope
- Base `0x140` (`bit8+bit6`) corresponds to `state8+6` positive threshold path.
- High bits `21,26,29,30,31` (0x00200000,0x04000000,0x20000000,0x40000000,0x80000000) over base `0x140` with low bits `1,2,4` (`0x02,0x04,0x10` and combos `0x06,0x12,0x14,0x16`).
- Low variants (`has_low = (combined & 0x16)!=0`) have uniform dispatcher excess `+3` unilateral / `+6` bilateral across `cd 0/1, mode6 0/1, t 0..2` (measured via `validate_game_info_full_dispatch.py`).
- Base without low (`has_low==0`) has `cd0/mode6=1` split (+8 for f0, +4 for f1, +7 bilateral) vs uniform otherwise — kept fail-closed pending full matrix.

## Measured cases
- `0x04000140` (high26) `0/36` with mode6 split (f0 cd0 m1 +8 else +3; bi cd0 m1 +7 else +6).
- `0x04000142` (high26+low2) `36/36` after `-3/-6` — uniform (f0 +3 all, bi +6 all) across 36 combos.
- `0x24000142` (high26+29+low2) `36/36` after `-3/-6` — uniform.
- `0x24200142` (triple 21+26+29+low2) `36/36`, `0x64200142` `36/36`, `0xE0200142` `36/36`.
- `0xE4200142` (quint all 5 highs+low2) `18/36` with uniform `-3/-6` — excluded, remains `0/36` until its `18/36` split is measured.

## Coverage
- v0242: 5 highs ×7 low =35 masks (singles low)
- v0243: 10 highs ×7 low =70 masks (pairs low)
- v0244: 14 highs ×7 low =98 masks (triples/quads low, excludes E4000140 already 8 in v0241 and quint)
- Total admitted this batch: 203 masks, all `36/36 exact` at `t 0..2, cd 0/1, mode6 0/1`.
- Grand total: `871 -> 1074` (120 high-family earlier + 954? actually 871+203=1074).

## Recovery predicates in `src/recovered/hybrid.c`
- `v0242` single low: `(combined & ~0x16) in {0x00200140,0x04000140,0x20000140,0x40000140,0x80000140} && has_low` subtract `6/3`.
- `v0243` pair low: 10 masks `0x04200140` etc && has_low subtract `6/3`.
- `v0244` triple/quad low: 14 masks `0x24200140` etc && has_low subtract `6/3`.
- All set `compare LESS/EQUAL` from `countdown_was_nonzero` and `stale_low` via `hybrid_set_stale_low`.

## Remaining for 100%
- Base `0x140` without low: 5 singles +10 pairs +14 triples/quads =29 masks `0/36` with mode6 split.
- Quint `0xE4200140` (7 low) `0/36` (18/36 with uniform) needs separate measurement.
- Full high-family for base `0x140` is 31 highs ×8 =248; admitted 203 low +8 (E400) =211, so 37 remain.
