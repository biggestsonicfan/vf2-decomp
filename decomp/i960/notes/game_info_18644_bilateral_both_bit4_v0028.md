# fa_game_info `0x18644` bilateral both-bit4 validation (v0.0.28)

The previously fail-closed bilateral both-bit4 corridor is ROM-backed with both fighter state words equal to `0x00000110`. The controlled differential starts from the real caller boundary at `0x000164ac`, immediately before the CALL to `0x00018644`. Because the ROM begins the child with `ld 0x1a4(g7), r7` and `ld 0x1a4(g8), r8`, the probe writes the state at each fighter's `+0x1a4` field; it does not synthesize `r7/r8` in the caller snapshot.

The recovered child is followed by the unchanged ROM tail to the task return at `0x00010dcc`, where the complete CPU and mutable-memory snapshot is compared with a pure-ROM execution from the same caller checkpoint. The calibrated instruction/call/return counters are checked separately as well.

| countdown | mode bit 6 | caller-to-task instructions | result |
| --- | --- | ---: | --- |
| 0 | clear | 202 | MATCH |
| 0 | set | 207 | MATCH |
| 1 | clear | 202 | MATCH |
| 1 | set | 207 | MATCH |

All four cases match complete serialized CPU state, mutable memory, instruction count, procedure calls and procedure returns. This supersedes the earlier synthetic caller-register measurements of 212/217 and 188/193.

This recovery remains intentionally narrow: it admits exactly the bilateral both-bit4 state `0x00000110/0x00000110`. Other mixed-extra-state combinations remain separate recovery boundaries until independently measured.
