# `fa_game_info` `0x1645c`: state-4 flag-domain hardening

## Result

The complete committed state-4 oracle matrix proves all 16 combinations of
flag bits 6, 14, 15 and 16 across fighter-0-only, fighter-1-only and bilateral
distributions, both countdown values and both mode-bit-6 values. Those 192
fixtures remain fully admitted.

The native predicates, however, previously tested only the conditional bits
needed by each corridor. An unrelated bit outside the measured four-bit domain
could therefore hitchhike on a neutral, isolated or composed state-4 admission
without any ROM-backed evidence.

v0130 adds one shared `measured_state4_flag_domain` guard. Native state-4
admission now requires the combined `+0x1a4` flags to contain no bits outside
6, 14, 15 and 16. The individual 16-mask semantics and accounting rules are
unchanged. Unknown extra bits remain on the existing conservative dispatcher
fallback.

This is a fail-closed evidence-boundary correction. It does not claim new ROM
semantics and does not retire any member of the proven state-4 matrix.

## Validation

The one-shot application gate builds and runs CTest with GCC, Clang, and Clang
ASan/UBSan before committing the change. ROM-derived artifacts are neither
created nor committed.
