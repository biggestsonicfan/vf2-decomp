# v0180: state-4 compound bit-14+bit-15 isolated-high extensions

Entry `0x0001645c`, both fighter state bytes 4, measured unilateral/bilateral fighter-record distributions.

The next regular high-extension family combines one isolated high bit (21, 26, 29, 30 or 31) with either low bits 14+15 (`0x0000c000`) or low bits 14+15+16 (`0x0001c000`). Both low compositions share the same dispatcher join and architectural tail.

## Recovered rule

For thresholds `0..2`:

- unilateral fighter0-only/fighter1-only adds two recovered instructions;
- bilateral adds four recovered instructions;
- compare state is `LESS` regardless of countdown;
- stale-frame `r3=0x41000000`, `r4=0x07800f0f`, `r7=0x41000000`;
- when fighter 1 participates, stale-frame `r15=1`.

No mutable Model 2 RAM correction is required. Threshold `-1` was already exact and remains outside the predicate.

## Verification

Fresh V2.2 ROM-backed full-dispatch validation covered ten masks x three distributions x two countdown values x two mode-bit-6 values at thresholds `-1`, `0`, `1`, and `2`: **480/480 exact snapshots** after the patch.

The strict warnings-as-errors build and complete local CTest suite pass. Remaining state-4 high-extension families include bit6-bearing and bit15+bit16 compositions, which require different accounting relations.
