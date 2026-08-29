# v0179: state-4 bit-15 isolated-high extensions

Entry `0x0001645c`, both fighter state bytes 4, measured unilateral/bilateral fighter-record distributions.

The next post-v0178 state-4 family is low bit 15 (`0x00008000`) combined with one isolated high bit 21, 26, 29, 30 or 31. All five masks share the same postconditions; unlike bit 14/16, bit 15 leaves the comparison in `LESS` regardless of countdown and has a distinct bilateral accounting join.

## Recovered rule

For thresholds `0..2`:

- fighter0-only and fighter1-only add one recovered instruction;
- bilateral adds two recovered instructions;
- compare state is `LESS` for both countdown values;
- stale-frame `r3=0x41000000`, `r4=0x07800f0f`, `r7=0x41000000`;
- when fighter 1 participates, stale-frame `r15=0x80004400`.

No mutable Model 2 RAM correction is required. Threshold `-1` was already exact and remains outside the postcondition predicate.

## Verification

Fresh V2.2 ROM-backed full-dispatch validation covered five masks x three distributions x two countdown values x two mode-bit-6 values at each threshold `-1`, `0`, `1`, and `2`: **240/240 exact snapshots** after the patch.

The strict warnings-as-errors build and complete local CTest suite pass. Compound low-bit state-4 families remain the next measured high-extension frontier.
