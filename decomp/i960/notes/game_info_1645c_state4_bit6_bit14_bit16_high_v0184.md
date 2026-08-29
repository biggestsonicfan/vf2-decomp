# v0184: state-4 bit6+bit14 / bit6+bit16 isolated-high extensions

Entry `0x0001645c`, both fighter state bytes 4, measured unilateral/bilateral fighter-record distributions.

This recovery covers low families `bit6+bit14` and `bit6+bit16` combined with one isolated high bit 21, 26, 29, 30, or 31.

For nonnegative shared thresholds, both families have the same measured tail:

- add one native instruction for unilateral and bilateral distributions;
- compare is countdown-derived (`EQUAL` when zero, `LESS` when nonzero);
- restore stale-frame `r3=0x41000000`, `r4=0x07800f0f`, `r7=0x41000000`;
- set stale-frame `r15=1` when fighter 1 participates;
- no mutable Model 2 RAM correction is required.

Threshold `-1` remains outside the correction predicate and was used as a negative control.

## Verification

Fresh VF2 V2.2 ROM-backed differential validation covered 10 masks x 3 distributions x 2 countdown values x 2 mode-bit-6 values x 4 thresholds (`-1`, `0`, `1`, `2`): **480/480 exact snapshots** after the patch. The strict warnings-as-errors build and local CTest suite also pass.
