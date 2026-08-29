# v0176: close the remaining positive bit-6 high-extension matrix

Entry `0x0001645c`, both fighter state bytes 8, measured matrix distribution, nonnegative shared threshold.

After v0175, a fresh full-dispatch screen left exactly 51 inexact masks. They were concentrated in the `0x4140`, `0x8140`, and `0x10140` low families:

- 15 `0x4140` masks: high bit 21 plus every non-empty subset of high bits 26/29/30/31;
- 15 `0x8140` masks: the same high-21 cross extensions;
- 21 `0x10140` masks: all 15 high-21 cross extensions plus six no-high-21 combinations containing high bit 31 together with high bit 29 and/or 30.

The six no-high-21 `0x10140` masks are `0xa0010140`, `0xc0010140`, `0xa4010140`, `0xc4010140`, `0xe0010140`, and `0xe4010140`. A previous note described their conceptual family with malformed nine-digit constants; the fresh executable harness confirms the real 32-bit masks above were still inexact, so this slice fixes the implementation rather than preserving that documentation error.

## Recovered postconditions

Admission is structural and remains fail-closed outside the measured state-8/state-8, unilateral/bilateral, nonnegative-threshold domain.

- `0x4140` high-21 cross extensions add 2 instructions unilateral / 4 bilateral.
- `0x8140` high-21 cross extensions subtract 3 unilateral / 5 bilateral.
- `0x10140` extensions containing high bit 29 add 3 unilateral / 6 bilateral.
- `0x10140` extensions without high bit 29 add 4 unilateral / 8 bilateral.
- all handled masks restore countdown-derived `EQUAL`/`LESS` compare state and the measured stale local-frame register pattern.
- handled `0x10140` masks OR fighter `+0x1a4` bit 11; when high bit 29 is present they also write `0x1e` to fighter `+0x6da`.

## Verification

Fresh V2.2 ROM-backed full-dispatch validation covered all 51 masks across 3 fighter distributions, 2 countdown states, 2 mode-bit-6 states, and thresholds 0, 1, and 2: **1,836/1,836 exact** (`51 masks x 36 fixtures ). Each case compared complete serialized snapshots, including CPU/architectural state, mutable Model 2 memory, execution counters, and interrupt counters.

Combined with v0175, the fresh 171-mask screen is now fully closed: **171/171 previously inexact masks recovered**, with **6,156/6,156** new full-dispatch fixtures exact across the two slices.

This completes the positive state-8 bit-6 high-extension frontier identified by the post-v0174 screen. The next recovery target should be selected from fresh runtime/state-space evidence rather than additional combinations in this now-closed family.
