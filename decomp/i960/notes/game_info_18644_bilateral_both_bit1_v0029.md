# fa_game_info `0x18644` bilateral both-bit1 validation (v0.0.29)

The previously fail-closed bilateral both-bit1 corridor is now ROM-backed with both fighter state words equal to `0x00000102`. The probe starts at the real caller boundary `0x000164ac` and writes `0x00000102` to each fighter's `+0x1a4` state field, matching the ROM's opening loads at `0x18648` and `0x1864c`.

Opening only the two conservative state guards produced exact CPU and mutable-memory snapshots but exposed a pure instruction-accounting deficit: 9 instructions with zero countdown and 11 with nonzero countdown. Adding those exact bilateral both-bit1 deltas closes the complete matrix:

| countdown | mode bit 6 | caller-to-task instructions | result |
| --- | --- | ---: | --- |
| 0 | clear | 232 | MATCH |
| 0 | set | 237 | MATCH |
| 1 | clear | 208 | MATCH |
| 1 | set | 213 | MATCH |

All four cases match complete serialized CPU state, mutable memory, instruction count, procedure calls and procedure returns at `0x00010dcc`. The recovery is exact-state only: `0x00000102/0x00000102` is admitted without generalizing unrelated bit1 mixtures.
