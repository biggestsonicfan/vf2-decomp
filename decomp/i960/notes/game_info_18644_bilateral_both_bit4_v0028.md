# fa_game_info `0x18644` bilateral both-bit4 validation (v0.0.28)

The previously fail-closed `r7 = r8 = 0x00000110` corridor is now ROM-backed. The differential starts from the real caller boundary at `0x000164ac`, immediately before the CALL to `0x00018644`, and edits architectural `r7/r8` directly. The recovered child is then followed by the unchanged ROM tail to the task return at `0x00010dcc`, where the resulting snapshot is compared against a pure-ROM execution from the same caller snapshot.

The second candidate had to cross two explicit guards: the mixed bilateral state gate and the later `0x189d0` threshold boundary. Allowing only the exact `0x00000110/0x00000110` state through both boundaries produces exact final snapshots in all four controlled cases:

| countdown | mode bit 6 | caller-to-task instructions | result |
| --- | --- | ---: | --- |
| 0 | clear | 212 | MATCH |
| 0 | set | 217 | MATCH |
| 1 | clear | 188 | MATCH |
| 1 | set | 193 | MATCH |

The comparisons include the complete serialized CPU and mutable-memory snapshot at `0x00010dcc`; the calibrated recovered path also reaches the same final instruction counter as the ROM reference in every case.

This recovery is intentionally narrow. It admits exactly the bilateral both-bit4 state `0x00000110/0x00000110`. Other previously unmeasured bilateral same-extra-state or mixed-extra-state combinations remain separate recovery boundaries until independently measured.
