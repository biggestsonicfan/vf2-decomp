# v0238 high pairs low variants for 0x1C140/0x14140

Extends `0` excess high-pair admissions to full low cubes.

## Measurement
* `0x2401C140` etc base `36/36` with `0/0`, low `|0x02` etc `0/36` with `-2` uni / `-5` bi (1C140) and `-2`/`-4` (14140), snapshot ARCH DIFF (stale) + bit11 missing at `fighter+0x1a5`
* No `0x6da` write for these families — unlike 10140/18140 bit29 groups

## Recovery
* 1C140: `11 bases ×7 low =77` with `+5` bi / `+2` uni
* 14140: `10 bases ×7 low =70` (E4014140 quad remains fail-closed) with `+4` bi / `+2` uni

Predicates `& ~0x16 == base && low!=0` plus stale and bit11. All `147` masks `36/36 exact`.

## Totals
* 681→828 masks (`120` + `708`)
* 55/55 CTest pass.
