# v0188: state-4 bit15 pair-high poststate

Entry `0x0001645c`, both fighter state bytes 4. This recovery covers low bit15 alone (`0x8000`) combined with exactly two high bits among 21, 26, 29, 30, and 31.

ROM-backed measurement shows the task already returns through the ordinary scheduler-return address `0x00010dcc`, but the native poststate is short by one instruction for unilateral distributions and two instructions for bilateral distribution. The final compare state is always `LESS`.

The stale local frame two levels above the returned frame must restore `r3=0x41000000`, `r4=0x07800f0f`, and `r7=0x41000000`. When fighter 1 participates, stale-frame `r15=0x80004400`. No mutable Model 2 RAM correction is required.

## Verification

All 10 high-bit pairs x 3 fighter distributions x 2 countdown values x 2 mode-bit-6 values x thresholds 0/1/2 pass: **360/360 exact ROM-backed snapshots**.

Threshold `-1` remains outside the new predicate and passes as a negative control across the same pair/distribution/countdown/mode matrix: **120/120 exact snapshots**.

The standard local CTest gate passes **23/23**.
