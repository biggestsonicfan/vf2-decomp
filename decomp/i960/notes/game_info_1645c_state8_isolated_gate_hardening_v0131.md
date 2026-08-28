# `fa_game_info` `0x1645c`: state-8 isolated-gate hardening

## Result

The generic positive state-8 predicates for bits 14, 15 and 16 were broader
than the evidence represented by their names. Each gate required its target
bit and excluded only bits 6/14/15/16 as appropriate, so unrelated low or
high `+0x1a4` bits could hitchhike on an isolated native admission.

That bypassed the exact-mask predicates used by the later ROM-measured
high-bit and cross-family families. The historical full-dispatch bit-14
measurement explicitly records isolated-bit coverage and keeps other positive
compositions as evidence boundaries; the specialized families likewise state
that admission is limited to their measured masks.

v0131 makes the generic gates literal: their combined fighter flags must be
exactly bit 14, exactly bit 15, or exactly bit 16. The three measured fighter
distributions remain representable because each record may carry the same
single bit or zero while the combined mask remains exact. Specialized measured
compositions continue through their existing exact predicates. Unsupported
compositions remain on the conservative dispatcher fallback.

This is a fail-closed boundary correction; it does not add new ROM semantics.

## Validation

The one-shot application gate builds and runs CTest with GCC, Clang, and Clang
ASan/UBSan before committing. It also asserts that representative specialized
state-8 exact-mask predicates and the conservative fallback remain present.
No ROM-derived artifact is created or committed.
