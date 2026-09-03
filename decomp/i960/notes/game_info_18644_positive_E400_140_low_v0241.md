# v0241 high quad E400 with base 0x140

`0xE4000140` = high `26+29+30+31` + `0x140` (`state8+6`).

## Measurement
* `0xE4000140` `0/36` with `+0` but `snapshot DIFF` at `fighter+0x1a5` (`1` vs `9`) and `fighter+0x6da` (`0` vs `0x1e`) — native over-wrote `bit11`/`0x1e` where ref has not.
* Low variants `|0x02..0x16` `0/36` with `+3` uni / `+6` bi (native long) + same snapshot diff.
* Fix: base `+0` + stale only, low `−3/−6` + stale, no `bit11`/`0x1e`.

## Recovery
Predicate `(combined & ~0x16)==0xE4000140` covering 8 masks:

```
0xE4000140, 0xE4000142, 0xE4000144, 0xE4000146,
0xE4000150, 0xE4000152, 0xE4000154, 0xE4000156
```

with `has_low ? −6/−3 : 0`. All `8` `36/36 exact`.

## Totals
* 863→871 masks (`120` + `751`)
* 55/55 CTest pass.
