# v0177: state-4 isolated high-bit postconditions

Entry `0x0001645c`, both fighter state bytes 4, measured unilateral/bilateral fighter-record distributions.

A fresh post-v0176 state-space screen extended the state-4 matrix beyond the previously recovered bits 6/14/15/16 and tested isolated high bits 21, 26, 29, 30 and 31. All five masks reached the native child but left the same small architectural tail mismatch at nonnegative thresholds.

## Recovered family

The five isolated state-4 field masks are:

- `0x00200000` (bit 21)
- `0x04000000` (bit 26)
- `0x20000000` (bit 29)
- `0x40000000` (bit 30)
- `0x80000000` (bit 31)

For thresholds `0..2`, every measured distribution shares one postcondition rule:

- add one recovered instruction to dispatcher accounting;
- restore countdown-derived compare state (`EQUAL` when countdown is zero, `LESS` when nonzero);
- restore stale local-frame registers `r3=0x41000000`, `r4=0x07800f0f`, and `r7=0x41000000`.

No mutable Model 2 RAM bytes differ for this family. Threshold `-1` was already exact and remains outside the correction predicate.

## Verification

Fresh V2.2 ROM-backed full-dispatch validation covered all five masks across three fighter distributions, both countdown values, both mode-bit-6 values and thresholds `-1,0,1,2`: **240/240 exact snapshots** after the patch.

The strict CMake build with warnings-as-errors and the complete local CTest suite also pass. The next state-4 frontier is the high-bit extensions combined with the recovered low flag families, which the same screen shows as systematic but composition-dependent accounting/postcondition gaps.
