# v0313: coli helper `0x23238` float-threshold path

## Verdict

`0x23238` is now fully native on the measured shapes:

| Condition | g0 out | body |
|-----------|--------|------|
| `g0 != 0x2ce` | unchanged | 2 |
| `g0 == 0x2ce`, `g8+0x1f8 > 0.9f` | unchanged `0x2ce` | 6 |
| `g0 == 0x2ce`, `0.6f < g8+0x1f8 ≤ 0.9f` | `0xa7` | 10 |
| `g0 == 0x2ce`, `g8+0x1f8 ≤ 0.6f` | `0x2cf` | 11 |

Constants `0x3f666666` (~0.9) and `0x3f19999a` (~0.6) from the ROM.

## Drive

From `out/v310/at-230d4.vf2snap` with bit 26 set, stop at `0x23238`,
`--set-reg g0=0x2ce`. Park `g8+0x1f8 = 0` → low path, 12 insns
including ret.

## Proof

- unit test: early-out `3/0/1`; low `12/0/1` `g0=0x2cf`; mid
  `11/0/1` `g0=0xa7`; high `7/0/1` `g0` unchanged
- ctest Debug **56/56**
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
