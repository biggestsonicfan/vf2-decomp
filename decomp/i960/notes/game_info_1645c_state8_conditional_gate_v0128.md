# `fa_game_info` `0x1645c`: state-8 conditional gate hardening

## Result

The three generic positive dispatcher predicates for isolated conditional bits
14, 15 and 16 used `fighter_state != 4`. That was broader than the evidence:
the full-dispatch measurements and the later family audit are explicitly state 8,
while the bit-31 task caller itself is selected from fighter flags and does not
constrain the byte at `fighter + 0xa00`. Consequently values other than 4 or 8
could silently enter the recovered `0x18644` corridor.

v0128 changes only that state gate. `native_bit14_fighter_path`,
`native_bit15_fighter_path` and `native_bit16_fighter_path` now require both
fighter state bytes to equal 8. Their existing state-8 flag-mask and threshold
behavior is unchanged. State-4 paths continue through their separately measured
predicates, and every other state value fails closed.

This is a boundary correction, not a claim of new game semantics. It aligns the
implementation with the committed full-task evidence, including the isolated
bit-14 dispatcher measurement and the v0124 audit of remaining state-8
admissions.

## Validation

The one-shot application gate builds and tests the modified tree with GCC,
Clang, and Clang ASan/UBSan before committing it. No ROM-derived artifact is
created or committed. ROM-backed full-dispatch matrices remain the authority for
any future widening of these gates.
