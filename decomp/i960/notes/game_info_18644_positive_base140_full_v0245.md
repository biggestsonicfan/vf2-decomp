# game_info 0x18644 positive base 0x140 full high-family — v0245-v0246

## Scope
- Base `0x140` (`bit8+bit6`) high-family over 5 highs `21,26,29,30,31` =31 subsets ×8 low =248 masks.
- Previously 211/248 admitted (v0241 8 + v0242 35 + v0243 70 + v0244 98). Remaining 37: 30 base-no-low +7 quint low.

## Measured cases
- Base-no-low `0x04000140` etc `0/36` with cd0/mode6 split: f0 cd0m1 +8 else +3; f1 cd0m1 +4 else +3; bi cd0m1 +7 else +6 (measured 36/36 after conditional).
- `0x24000140` `36/36` after conditional; `0xE4200140` base `36/36` after conditional (mode6 split).
- Quint low `0xE4200142` `0/36` uniform `+6` gives `18/36`; measured cd split: cd0 +8 uni / +11 bi, cd1 +3/+6 (36/36 after cd-dependent).

## Recovery
- v0245: 30 bases without low `& ~0x16==high|0x140 && has_low==0 && high in 30-set` with `cd0m1` conditional excess.
- v0246: quint low `& ~0x16==0xE4200140 && has_low` with `cd` conditional: `cd1 3/6, cd0 8/11`.
- All set `LESS/EQUAL` from `countdown_was_nonzero` and `stale_low`.

## Coverage
- 30 +7 =37 masks `36/36 exact`.
- High-family base 0x140 now `248/248` exact across 36-case matrices.
- Grand total `1074 -> 1111` (120 high-family earlier + 991 base/low/high).
