# fa_game_info type-22 equal path (`0x18bd4`)

Controlled ROM probes distinguish the synthetic sentinel case from a coherent type-22 record. The type byte at `fighter1+0x19f` is the high byte of the 32-bit descriptor at `+0x19c`; forcing only the byte produced `0x16000000`, whose low 13-bit handle is zero and resolves through table entry zero to the `0x09090000` sentinel. A coherent descriptor `0x1600000d` resolves handle 13 through `0x0200d34c` to a type-5 record at `0x02014676`.

With fighter0 `+0x1aa == +0x80a - 1` and the measured r9 threshold selecting the call, the ROM executes `0x18a4c -> 0x18bd4 -> 0x1ab34`, then `0x18c04 -> 0x18b58`, and returns to `0x16500`. Relative to the type-22 mismatch corridor, the coherent call path adds exactly 55 instructions and 3 calls/3 returns. The frame totals are 736 instructions/15 calls versus 681/12 for the mismatch probe. The visible writes are `fighter0+0x198 = 0x11000064` (type-5 payload value `0x64`) and `fighter1+0x198 = 0x1000000d` (the original handle); the remaining flag/state writes are idempotent in the measured state.

The native recovery implements the generic `0x1ab34` record walk and the measured bit-2-clear `0x18b58` subpath. The bit-2-set directions remain explicitly unsupported until separately measured.

Post-recovery differential validation also confirmed the terminal `CHKBIT 10` condition state: with fighter1 bit10 set the helper exits with `compare=EQUAL` and arithmetic-control condition bits `2`, exactly as the ROM.
