# `fa_game_info` `0x1645c`: state-4 gate hardening

## Result

The native state-4 admissions were broader than their committed oracle evidence.
The state-4 fixture and validator establish state 4 for both fighter records, but
the recovered dispatcher predicates used `fighter0_state == 4 ||
fighter1_state == 4`. That admitted mixed state pairs that were never measured
against the ROM.

v0129 narrows only native state-4 admissions to the measured bilateral domain:
both fighter state bytes must equal 4. This applies to the neutral, isolated and
composed bit-6/14/15/16 state-4 predicates. The flag-mask logic inside each
predicate is unchanged.

The broad state-4 checks used to select conservative fallback behavior remain
unchanged. In particular, mixed state pairs still cause the dispatcher to avoid
claiming unsupported native semantics; they are not reclassified as a recovered
path.

This is a fail-closed boundary correction, not a claim about new game behavior.
Future widening requires an explicit ROM-backed mixed-state matrix.

## Validation

The one-shot application gate builds and runs CTest with GCC, Clang, and Clang
ASan/UBSan before committing the change. No ROM-derived artifact is created or
committed.
