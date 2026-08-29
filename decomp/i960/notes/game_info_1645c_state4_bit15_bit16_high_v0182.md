# v0182: state-4 bit-15+bit-16 isolated-high extensions

Entry `0x0001645c`, both fighter state bytes 4, measured unilateral/bilateral fighter-record distributions.

This family combines low bits 15+16 (`0x00018000`) with one isolated high bit 21, 26, 29, 30 or 31. It exposes both architectural and fighter-memory postconditions.

## Recovered rule

For nonnegative thresholds:

- high 21/26/30/31 add four recovered instructions unilaterally and eight bilaterally;
- high 29 adds three recovered instructions unilaterally and six bilaterally;
- compare state is always `LESS`;
- stale-frame `r3=0x41000000`, `r4=0x07800f0f`, `r7=0x41000000`;
- when fighter 1 participates, stale-frame `r15=1`;
- selected fighter records OR bit 11 into `fighter+0x1a4`;
- high 29 additionally writes byte `0x1e` to selected `fighter+0x6da`.

Threshold `-1` was already exact and remains outside the correction predicate.

## Verification

Fresh V2.2 ROM-backed full-dispatch validation covered five masks x three distributions x two countdown values x two mode-bit-6 values at thresholds `-1`, `0`, `1`, and `2`: **240/240 exact snapshots** after the patch.

The strict warnings-as-errors build and complete local CTest suite pass.
