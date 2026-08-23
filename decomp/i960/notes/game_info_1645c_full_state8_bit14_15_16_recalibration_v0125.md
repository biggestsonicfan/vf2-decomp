# `fa_game_info` `0x1645c`: recalibration of the bit-14+15+16 family and the bit-21 compounds

## Summary

The eleven masks retired in v0124 were re-measured under the committed
harness with zero-correction deficit matrices (three distributions ×
countdown 0/1 × mode bit 6 clear/set, each cell verified across thresholds
0..2). Nine of them are now re-admitted with exact 36-case matrices; two
remain retired with a genuine unrecovered memory divergence.

## Admitted: bit-14+bit-15+bit-16 base family (7 masks)

Masks `0x0021C000`, `0x0421C000`, `0x2021C000`, `0x4021C000`,
`0x8021C000`, `0x2421C000`, `0xE421C000`. Measured corrections:

- fighter-0-side term: `3`, or `2` when `countdown != 0 || mode6`;
- fighter-1-side term: `3 + 5*mode6 + 4*cd − 4*cd*mode6`;
- bilateral for the six compositions that add a high bit beyond 21:
  exactly the sum of both terms (`{6, 10, 9, 10}` over the cd×m6 matrix);
- the isolated `0x0021C000` measures a flat bilateral table
  `{4, 8, 7, 8}` instead and keeps its own complete table;
- stale-frame postconditions follow the shared family clause.

## Admitted: two-bit bit-21 compounds (2 of 4 masks)

- `0x00204000` ({14,21}): fighter-0 side constant `+2`; fighter-1 side and
  bilateral share `{2, 3, 7, 8}` over cd×m6.
- `0x00208000` ({15,21}): unilateral `{2, 2, −5?}` — precisely,
  unilateral `+2/+2/−2/−2` and bilateral `+3/+3/−5/−5`; notably its stale
  `r15` postcondition is `0` (not the shared `1`) in every distribution
  touching fighter 1.

## Still retired (fail closed): the bit-16 compounds

`0x00210000` ({16,21}) and `0x00218000` ({15,16,21}) show one unrecovered
work-RAM byte in the recovered child corridor: the reference sets bit 15 of
the word at `fighter + 0x...b24` (byte at offset `+0xb25`, e.g. expected
`0x002108xx` versus native `0x002100xx`), which the recovered path never
stores. Counters alone calibrate cleanly (unilateral `+4`, bilateral `+7`
and flat `+4`/`+8` respectively), but memory equality is required before
admission. Recovering that store is the next targeted step.

## Controls

- Retired masks refuse: both bit-16 compounds report 0/36.
- Threshold 3 refuses; outside-family neighbours refuse.
- Regression: ctest 51/51; child-level state-8 matrix 96/96; state-4 matrix
  192/192; spot checks across {14,16} and {14,15} families all 36/36.
