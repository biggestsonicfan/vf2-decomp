# `fa_game_info` `0x1645c`: matrix-distribution hardening

## Result

The committed ROM matrices do not cover arbitrary partitions of a flag mask
between the two fighter records. For each non-zero mask they measure exactly
three record distributions: fighter 0 carries the complete mask, fighter 1
carries the complete mask, or both carry the same complete mask.

Several generic dispatcher predicates previously checked only the OR-combined
mask. That allowed an unmeasured split such as bit 14 on fighter 0 and bit 15
on fighter 1 to masquerade as a measured bit-14+bit-15 composition. The same
issue existed in the generic positive state-8 bit-6 gate and in negative-
threshold state-8/state-4 accounting.

v0132 introduces a shared `measured_matrix_distribution` predicate requiring
each fighter flag word to be either zero or the complete combined mask. It is
applied to every state-4 native gate and to the generic positive state-8 bit-6
gate. At negative thresholds, any state-4/state-8 pair outside the bilateral
state byte and three-distribution domains now takes the complete interpreted
task fallback. Negative accounting and stale-frame corrections are likewise
restricted to the bilateral state domains that were actually measured.

All previously measured fighter0-only, fighter1-only and bilateral fixtures
remain admitted. No exact-mask specialized state-8 family is changed.

This is a fail-closed evidence-boundary correction; it adds no new ROM
semantics.

## Validation

The one-shot application gate builds and runs CTest with GCC, Clang, and Clang
ASan/UBSan before committing. Structural assertions require the expected state-4
gates, generic bit-6 gate, negative accounting gates, and stale-frame gate to
be updated together. No ROM-derived artifact is created or committed.
