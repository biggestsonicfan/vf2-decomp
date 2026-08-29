# v0183: state-4 bit-6 isolated-high extensions

Entry `0x0001645c`, both fighter state bytes 4, measured unilateral/bilateral fighter-record distributions.

After closing every state-4 high-extension family that does not contain low bit 6, the remaining matrix collapses onto bit-6-bearing families. The simplest is low bit 6 (`0x00000040`) combined with one isolated high bit 21, 26, 29, 30 or 31.

## Recovered rule

For nonnegative thresholds:

- unilateral fighter0-only/fighter1-only accounting was already exact;
- bilateral execution carries one excess native instruction, which is subtracted with a fail-closed underflow guard;
- compare state is countdown-derived (`EQUAL` at zero, `LESS` when nonzero);
- stale-frame `r3=0x41000000`, `r4=0x07800f0f`, `r7=0x41000000`;
- when fighter 1 participates, stale-frame `r15=1`.

No mutable Model 2 RAM correction is required. Threshold `-1` was already exact and remains outside the correction predicate.

## Verification

Fresh V2.2 ROM-backed full-dispatch validation covered five masks x three distributions x two countdown values x two mode-bit-6 values at thresholds `-1`, `0`, `1`, and `2`: **240/240 exact snapshots** after the patch.

The strict warnings-as-errors build and complete local CTest suite pass.
