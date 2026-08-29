# v0181: state-4 bit-14+bit-16 isolated-high extensions

Entry `0x0001645c`, both fighter state bytes 4, measured unilateral/bilateral fighter-record distributions.

The next regular state-4 family combines low bits 14+16 (`0x00014000`) with one isolated high bit 21, 26, 29, 30 or 31. Unlike the preceding families, the native path had excess instruction accounting rather than a deficit.

## Recovered rule

For nonnegative thresholds:

- unilateral fighter0-only/fighter1-only subtracts one excess native instruction;
- bilateral subtracts three excess native instructions;
- compare state is countdown-derived (`EQUAL` at zero, `LESS` when nonzero);
- stale-frame `r3=0x41000000`, `r4=0x07800f0f`, `r7=0x41000000`;
- when fighter 1 participates, stale-frame `r15=1`.

The subtraction is guarded fail-closed against counter underflow. No mutable Model 2 RAM correction is required. Threshold `-1` was already exact and remains outside the correction predicate.

## Verification

Fresh V2.2 ROM-backed full-dispatch validation covered five masks x three distributions x two countdown values x two mode-bit-6 values at thresholds `-1`, `0`, `1`, and `2`: **240/240 exact snapshots** after the patch.

The strict warnings-as-errors build and complete local CTest suite pass.
