# v0178: state-4 bit-14 / bit-16 isolated-high extensions

Entry `0x0001645c`, both fighter state bytes 4, measured unilateral/bilateral fighter-record distributions.

Following v0177, the next regular state-4 frontier is the pair of recovered low families bit 14 and bit 16 combined with one isolated high bit from 21, 26, 29, 30 or 31. The fresh ROM-backed screen showed one shared dispatcher/postcondition rule across all ten masks.

## Recovered masks

Low bit 14 (`0x00004000`) plus one of:

- `0x00200000`, `0x04000000`, `0x20000000`, `0x40000000`, `0x80000000`.

Low bit 16 (`0x00010000`) plus the same five isolated high bits.

For nonnegative thresholds the recovered tail:

- adds one instruction in all three fighter distributions;
- restores countdown-derived `EQUAL` / `LESS` compare state;
- restores stale-frame `r3=0x41000000`, `r4=0x07800f0f`, `r7=0x41000000`;
- restores stale-frame `r15=1` when fighter 1 participates (fighter1-only or bilateral).

No mutable Model 2 RAM changes are required for these ten masks. Threshold `-1` was already exact and is deliberately left outside the correction predicate.

## Verification

Fresh V2.2 ROM-backed full-dispatch validation covered 10 masks x 3 distributions x 2 countdown values x 2 mode-bit-6 values at each threshold `-1`, `0`, `1`, and `2`: **480/480 exact snapshots** after the patch.

The strict warnings-as-errors build and complete local CTest suite pass. The next state-4 high-extension families remain composition-dependent, with bit 15 and compound low-bit families showing different instruction-accounting joins.
