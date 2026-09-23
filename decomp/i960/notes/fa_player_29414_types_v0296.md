# v0296: recover fa_player 0x29414 type-0/6/8/10 compact paths

## Verdict

`hybrid_execute_player_29414` (export `vf2_hybrid_player_29414_execute`)
now natively recovers:

- **type 0** — measured zero path (unchanged): 8 instructions / 0 calls /
  1 return, store 0 at `+0xc50`.
- **types 6 / 8 / 10 with state-flag bit 19 clear** — measured float tail:
  store `(r9 * scale - scale)` at `+0xc50`.

Bit-19-set siblings and the `+0x1aa < 20` window at `0x294ac..0x29534`
remain explicit `VF2_ERROR_UNSUPPORTED` boundaries.

## Measurement

Reference executor, park via `vf2probe --set-ip 0x00029414` from
`player-14288-rt` (`g7 = 0x00510980`). `--set-ip` was added in this slice
so a mid-corridor park can be forced without advancing the original IP.

Constant sets from ROM:

| Type | Site | r9 bits |
|------|------|---------|
| 6, 10 | `0x29478` | `0x3f7851ec` (~0.970) |
| 8 | `0x29430` | `0x3f6b851f` (~0.920) |

Measured body (bit-19 clear, `+0x84 = 2.0f`):

| Type | Insns to `ret` | Store bits at `+0xc50` |
|------|----------------|------------------------|
| 0 | 8 | `0x00000000` |
| 6 | 14 | `0xbd75c280` |
| 8 | 16 | `0xbe23d708` |
| 10 | 13 | `0xbd75c280` |

Float check: `0.970*2 - 2 = -0.060 → 0xbd75c280`;
`0.920*2 - 2 = -0.160 → 0xbe23d708`. Matches the reference stores exactly.

## Tooling

- `vf2probe --set-ip <address>` forces IP after restore (measurement only).
- `native_runtime.c` now treats `0x00014288` as a player mid-corridor
  continuation so `native-resume` can enter the hybrid player path from
  the parked `player-14288-rt` snapshot. PUNCH does not dispatch `0x14288`
  as a step entry, so the endurance pin is unchanged.

## Pins

- PUNCH **320/320 MATCH**, 12946 blocks, **14,962,620** insns, both `0x1645c`
- ctest Debug **56/56**
- unit test `test_player_29414_type_paths`

## Next

- Bit-19-set window (`0x294ac..0x294f4` indexed stores, `+0x1aa` cases)
  needs a live hybrid-context drive that can complete the interpreter
  fallback (the parked snapshot still faults at `0x2704c` on pure replay).
- `0x19ef8` flag-bit siblings remain blocked by the same interpreter fault.
