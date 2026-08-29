# v0175: recover remaining positive bit-6 cross-family high extensions

Entry `0x0001645c`, both fighter state bytes 8, measured matrix distribution, nonnegative shared threshold.

The normal sixth-dispatch snapshot was first resumed for more than 600,000 additional native blocks without reaching a genuine unsupported runtime boundary. The next useful frontier is therefore the still-inexact positive state-8 bit-6 matrix rather than the observed frame loop.

A complete one-case screen over the seven low families and every non-empty subset of high bits 21/26/29/30/31 found 171 inexact masks. This slice closes the four regular remaining low families `0xc140`, `0x14140`, `0x18140`, and `0x1c140` for every non-empty subset of high bits 26/29/30/31, both with and without high bit 21: 30 masks per family, 120 masks total.

## Recovered postconditions

The dispatcher now recognizes these families structurally instead of enumerating 120 exact constants. Admission still requires state-8/state-8, a measured unilateral/bilateral distribution, nonnegative threshold, and at least one of high bits 26/29/30/31; no unrelated flag bits are admitted.

- `0xc140` and `0x1c140`: add 2 instructions unilateral / 5 bilateral.
- `0x14140`: add 2 unilateral / 4 bilateral.
- `0x18140` without high bit 29: add 4 unilateral / 9 bilateral.
- `0x18140` with high bit 29: add 3 unilateral / 7 bilateral.
- all four families restore countdown-derived `EQUAL`/`LESS` compare state and the measured stale local-frame register pattern.
- `0x18140` also ORs fighter `+0x1a4` bit 11; when high bit 29 is present it writes `0x1e` to fighter `+0x6da`.

## Verification

Fresh V2.2 ROM-backed full-dispatch validation covered every new mask across 3 fighter distributions, 2 countdown states, 2 mode-bit-6 states, and thresholds 0, 1, and 2: **4,320/4,320 exact** (`120 masks x 36 fixtures ). Each case compared the complete serialized snapshot, therefore covering CPU/architectural state, mutable Model 2 memory, instruction/call/return counters, and interrupt counters.

Threshold 0 was checked as four 30-mask groups (`1,440/1,440`); thresholds 1 and 2 were each checked as two 60-mask groups (`1,440/1,440` each).

The warning-as-error Release build passed. The full ROM-backed CTest run passed through tests 1-44 before the local command budget expired; tests 45-52 were then rerun independently and passed 8/8, including `vf2_native_sixth_dispatch`, `vf2_native_seventh_dispatch`, texture/post-frame/geometry differentials, scheduler-entry differential, geometry helpers, and third-sweep observation.

This removes 120 of the 171 inexact masks found by the fresh cross-family screen and leaves 51 masks for the next recovery slice.
