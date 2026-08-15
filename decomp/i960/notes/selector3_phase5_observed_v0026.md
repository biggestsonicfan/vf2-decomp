# Selector-3 phase-5 observed path (v0.0.26)

This note records the controlled ROM corridor used to recover the observed selector-3 phase-5 entry (`0x0000b4f0`). Phase 5 is a reusable tail of phase 4 rather than another large setup block.

From framed `0x0000a6c0`, the observed route executes **81 instructions / 5 calls / 6 returns** with the call tree `0xa6c0 -> 0xacf8 -> 0xb4f0 -> 0x548cc -> 0x54be8 -> 0x1344`.

The vector enters with phase 5 and timer 2926 while the phase-4 total at `0x0201f388` is 2927. The `0x548cc` timeline compares elapsed time 1 against the four actor-list triggers. Only the list at `0x005001d0` matches: its descriptor at `0x0201f5c4` contains value `0x00000495`, trigger 1, and control byte 1. `0x54be8` writes `0x495` to fighter 0 offset `0x194`, clears fighter bit 26 because the control byte is nonzero, and advances the descriptor pointer by 8 to `0x0201f5cc`.

The full work-RAM delta for this route is only seven bytes: timer `0x00500024` decrements by one, wrapper state records phase 5 and mask `0x20`, `0x005001d0` advances by 8, fighter 0 bit 26 clears, and fighter 0 `+0x194` receives `0x0495`. The other actor/event timeline triggers are explicitly preflighted so the exact 81-instruction accounting is used only for this observed branch composition.

Implementation commit: `93e6754794c3d0aa8ec87326ee04f76106eeaf97` (`Close selector3 phase5 observed path`). Full native-vs-ROM snapshot validation is performed with the normal CI-built analysis binary.
