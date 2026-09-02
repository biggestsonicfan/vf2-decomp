# v0235 high-29 low-cube for 0x10140

Closes the lone `0/36` outlier noted in v0225.

## Measurement
* `0x20010140` isolated remained `0/36` with `counters -3` unilateral / `-6` bilateral and `work-ram diff at 0x510x6da` (`0x1e` vs `0x00`)
* Other highs `0x04010140`/`0x40010140`/`0x80010140` are `+8/+4` — high-29 is distinct `+6/+3`
* Adding `+3`/`+6` fixes instructions but leaves `snapshot DIFF arch DIFF` until `fighter+0x6da=0x1e` is reproduced for bit29 (same write already used for `positive_10140_composite` pairs/triples)
* `0x20010142..0x20010156` share same `-3/-6`, so cube holds

## Recovery
Single predicate `(combined & ~0x16)==0x20010140` covering 8 masks:

```
0x20010140, 0x20010142, 0x20010144, 0x20010146,
0x20010150, 0x20010152, 0x20010154, 0x20010156
```

with `+6` bilateral / `+3` unilateral, `LESS/EQUAL` via `countdown`, `hybrid_set_stale_low`, `fighter+0x1a4` bit11 and `fighter+0x6da=0x1e`.

Each `36/36 exact` via `validate_game_info_full_dispatch.py --mask 0x20010140 --state 8` etc.

## Totals
* 519→527 masks (`120` high family + `407`)
* 55/55 CTest pass; `vf2_native_seventh_dispatch` unchanged.
