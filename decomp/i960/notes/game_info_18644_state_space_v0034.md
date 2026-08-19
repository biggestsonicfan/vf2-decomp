# `fa_game_info` `0x18644`: ROM-backed state-space inventory

This note records a complete first-pass enumeration of fighter-state combinations built from state bit 8 plus optional bits 1, 2 and 4 at the real `fighter + 0x1a4` state source.

## Scope

The enumerated states are `0x100`, `0x102`, `0x104`, `0x106`, `0x110`, `0x112`, `0x114` and `0x116`.

Both fighters are enumerated independently, producing 64 ordered state pairs. Each pair is executed with countdown `0/1` and mode bit 6 clear/set, for 256 pure-ROM executions from caller boundary `0x000164ac` to scheduler return `0x00010dcc`. All 256 executions reached the scheduler return successfully.

## Result

The 64 ordered pairs collapse to nine distinct instruction-count vectors, ordered as `(countdown=0/mode6=0, countdown=0/mode6=1, countdown=1/mode6=0, countdown=1/mode6=1)`:

| class | vector | ordered pairs | recovered representative |
|---:|---|---:|---|
| 1 | `(197, 202, 194, 199)` | 8 | yes |
| 2 | `(202, 207, 202, 207)` | 4 | yes |
| 3 | `(208, 213, 205, 210)` | 16 | yes |
| 4 | `(210, 215, 186, 191)` | 4 | yes |
| 5 | `(213, 218, 213, 218)` | 8 | yes — `0x110<->0x112` |
| 6 | `(219, 224, 216, 221)` | 8 | yes — `0x102<->0x112` |
| 7 | `(221, 226, 197, 202)` | 8 | yes |
| 8 | `(224, 229, 224, 229)` | 4 | yes |
| 9 | `(232, 237, 208, 213)` | 4 | yes |

All nine discovered instruction-vector classes now have at least one ROM-differentially validated native representative. Eighteen of the 64 ordered pairs have been individually recovered and validated exactly; the remaining pairs stay fail-closed unless separately admitted.

Class 5 was closed by the exact representative `0x110<->0x112`; class 6, the final previously unrepresented class, was closed by `0x102<->0x112`. Both passed 16 isolated probes and the stronger eight-case chained proof with both `0x18644` invocations native.

## Reproducibility

`decomp/i960/enumerate_game_info_18644.py` reconstructs the calibrated fifth-dispatch caller boundary, reads fighter pointers from Work RAM, writes the real `fighter + 0x1a4` state fields, and emits the 256-case ROM matrix.

`decomp/i960/game_info_18644_state_vectors.csv` is the compact 64-pair inventory generated from this sweep.

Instruction-vector equivalence remains a prioritization signal, not proof of semantic equivalence. Exact promotion of another pair still requires the chained differential standard: native child #1, caller continuation, native child #2, ROM tail, then equality of CPU state, mutable memory, instruction count, calls and returns.
