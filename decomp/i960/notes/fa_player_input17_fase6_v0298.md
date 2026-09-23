# v0298b/Fase 6: input-driven witnesses distinct from PUNCH

## Verdict

Holding **input 17** (`PUNCH|bit0`) or **input 18** (`PUNCH|bit1`) from
`sixth-regen` opens a corridor that is not the warm PUNCH path.

| Input | 16 cycles from `sixth-regen` |
|-------|------------------------------|
| 16 (PUNCH) | 592 blocks / 68,815 insns **MATCH** |
| 17 | cycle 1 **MATCH** (37 blocks / 14,876); cycle 2 **UNSUPPORTED** after 71 blocks / 16,542 |
| 18 | same shape as 17 |
| 20/24/48 | same as PUNCH |
| 32/64 | 592 blocks but 34,684 insns (shorter path, MATCH) |

## Cycle-2 boundary (input 17)

```
Final reference/native address:  0x00009ff8 / 0x0000a6c0
```

- Reference is at `main_final_cluster` (`0x9ff8`).
- Native stops inside `frame_dispatch_tick` (`0xa6c0`) which does an
  indirect `callx` through the table at `0xa6f8[r3*4]`.
- Fail-closed is correct; the `callx` target family is not recovered.

## Parks (local only, not committed)

| File | Role |
|------|------|
| `out/v300/in17-c1.vf2snap` | after cycle-1 MATCH, ip `0x1645c`, g7 `0x510980` |
| `out/v300/in17-at-9ff8.vf2snap` | `native-resume --stop 0x9ff8` from c1 (34 blocks / 1668 insns) |

`vf2probe --set-ip`/`--until 0x9ff8` from the c1 park with `--input 17`
executes the measured prefix (54 insns to `0xa008`).

## Next Fase 6 steps

1. Trace the cycle-2 path from `in17-at-9ff8` with `--input 17` and
   attribute the `callx` targets.
2. Recover only compact measured leaves of that family.
3. Add a second endurance pin (e.g. `--input 17 --cycles 2`) as a
   **complement** to PUNCH, never a replacement.
4. Do not open hitbox/damage C until that pin is stable.

## Pins unchanged

- PUNCH **320/320 MATCH** / 12,946 blocks / 14,962,620 insns
- ctest Debug **56/56**
- No C recovery in this note.
