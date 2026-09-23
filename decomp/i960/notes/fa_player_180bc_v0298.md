# v0298: recover fa_player 0x180bc flag tail and 0x1441c task epilogue

## Verdict

The warm player path no longer needs `hybrid_execute_interpreted_task`
from `0x180bc`. Two compact C recoveries close the corridor:

1. `hybrid_execute_player_180bc` / `vf2_hybrid_player_180bc_execute`
2. `hybrid_execute_player_1441c_tail` / `vf2_hybrid_player_1441c_execute`

`0x14428` `ret` ends the player task at the scheduler return, so the
former interpreted tail through the scheduler is gone on the measured
shape.

## Tooling finding (Fase 5)

`hybrid.c` is compiled with `vf2_i960_run=vf2_hybrid_i960_run_tail`.
Every `hybrid_execute_interpreted_until(0x28178, 0x14400)` already
tries the recovered stream parser (`player_execute_28178_stream`) and
the `0x17710` / `0x1791c` / `0x4b640` semantic layers before falling
back to the architectural interpreter. On first-dispatch, `0x28178` is
only the 3-instruction thunk after `0x29414`; the long stream work
lives at `0x1abf8` inside the `0x1ab74` frame and is already owned by
that tail.

## Measured `0x180bc` (park `player-14288-rt`, `--set-ip 0x180bc`)

Warm sixth-shaped path: **18** instructions to `ret` at `0x18140`.

| Case | Body | Effects |
|------|------|---------|
| warm (`+0x1a4` 0, `+0x810` bit7 0) | 18 | `+0x5b4=+0x26`, clear flag bit8, `+0x6d8=0` |
| `+0x1a4` bit6 | 20 | `+0x5b4 = +0x26 + +0x812` |
| `+0x1a4` bit3 + `+0x810` bit7 | 17 | set flag bit8 |
| `+0x1a4` bit3, no bit7, fall to set | 21 | `cmpoble +0x1aa, +0x800>>1` |
| `+0x1a4` bit3, no bit7, branch to clear | 21 | same compare, other outcome |
| bit3 clear + bit7 + fall clear | 22 | clear flag bit8 |
| bit3 clear + bit7 + branch set | 21 | set flag bit8 |
| `+0x1a4` bit0 | 14 | skip `+0x6d8` |
| player-flags bit4 | 16 | skip `+0x6d8` |

Compare is signed (`cmpoble`); shift on `+0x800` is `shro` (logical) of
the sign-extended halfword.

## Measured `0x1441c`

```
1441c  ld (g7), r15
14420  setbit 7, r15, r15
14424  st r15, (g7)
14428  ret
```

4 instructions including the architectural `ret`.

## Pins

- PUNCH **320/320 MATCH**, 12946 blocks, **14,962,620** insns, both `0x1645c`
- ctest Debug **56/56**
- `vf2_native_sixth_dispatch` and player stream/rotation tests pass
- unit test `test_player_180bc_flag_tail`

## Next

- Fase 6: input-driven witness distinct from PUNCH (kick/block/throw)
  for collision/hitbox boundaries.
- Remaining player siblings (`0x19ef8` bits, `0x29414` type 0x2954 set)
  still blocked or unmeasured.
- Geometry helpers after `0x28780` are already native through
  `0x2826c`/`0x27d90`; further growth is the `0x1abf8` stream siblings
  when the tail falls back.
