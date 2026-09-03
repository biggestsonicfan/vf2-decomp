# v0239 high singles low variants for 0xC140

Base `0x0400C140` etc `36/36` with `0/0`, low `|0x02` etc `0/36` with `-2` uni / `-5` bi, stale ARCH DIFF + bit11 missing.

## Recovery
Single predicate covering 4 high singles (`0x0400C140`, `0x2000C140`, `0x4000C140`, `0x8000C140`) with `low!=0`, `+5` bi / `+2` uni, stale and bit11. All `28` masks `36/36 exact`.

## Totals
* 828→856 masks (`120` + `736`)
* 55/55 CTest pass.
