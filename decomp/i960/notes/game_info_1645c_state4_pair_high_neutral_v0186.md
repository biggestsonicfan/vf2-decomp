# v0186: state-4 neutral pair-high continuation

Entry `0x0001645c`, both fighter state bytes 4. This recovery admits the next fail-closed state-4 frontier: low conditional bits 6/14/15/16 all clear and exactly two high bits set among 21, 26, 29, 30, and 31.

ROM differential measurement shows that instruction accounting and mutable RAM are already exact for this family. The missing native contract is the dispatcher poststate: the task leaves compare state `NONE` and returns through scheduler epilogue entry `0x00010dd0`, rather than the ordinary `0x00010dcc` scheduler-return address.

The recovery therefore performs no instruction-count correction and no memory correction. It only restores the measured compare state and admits the measured continuation. The task wrapper now accepts `0x00010dd0` for `fa_game_info`; the inner recovery remains fail-closed and can produce that continuation only for explicitly recovered paths.

## Verification

Fresh VF2 V2.2 ROM-backed differential validation covered all 10 two-high combinations x 3 fighter distributions x 2 countdown values x 2 mode-bit-6 values x thresholds 0/1/2: **360/360 exact snapshots**.

Threshold `-1` was run as a negative-control matrix across the same 10 pairs x 3 distributions x 2 countdown x 2 mode values: **120/120 exact snapshots**, confirming the new nonnegative-threshold predicate does not perturb the existing negative path.

The local strict build succeeds and CTest passes **23/23**.
