# fa_game_info 0x18644 — cross bit1 / bit2 recovery (v0039)

The ordered state pairs `0x102 -> 0x104` and `0x104 -> 0x102` are now recovered natively and validated against the original ROM.

## State composition

- `0x102 = bit8 + bit1`
- `0x104 = bit8 + bit2`

The recovery remains fail-closed: only these two exact cross compositions were admitted at the bilateral composition gates.

## ROM behavioral vector

Both orientations belong to vector class 7:

- countdown=0, mode-bit6=0: 221 instructions
- countdown=0, mode-bit6=1: 226 instructions
- countdown=1, mode-bit6=0: 197 instructions
- countdown=1, mode-bit6=1: 202 instructions

## Validation

The reusable differential tooling in `decomp/i960/tools/` was used throughout.

Isolated child validation covered both call sites (`0x164b0` and `0x164c4`), both orientations, countdown 0/1 and mode bit 6 clear/set:

- 16/16 architectural snapshots matched the ROM
- executed-instruction deltas: 0 after final accounting
- procedure-call deltas: 0
- procedure-return deltas: 0
- interrupt-entry deltas: 0
- interrupt-return deltas: 0

The final strong chained validation executed:

`native child #1 -> caller ROM -> native child #2 -> ROM tail -> 0x10dcc`

across both orientations and all countdown/mode-bit6 combinations:

- 8/8 exact
- CPU and mutable memory matched
- instructions/calls/returns/interrupt counters all matched exactly

## Accounting note

The two orientations require different instruction corrections. The second call site uses the logical role-swap observed in the other recovered asymmetric/cross corridors, so the `0x164c4` correction is keyed to the state roles actually seen by the recovered child rather than naively reusing the initial caller orientation.

## Promotion

ROM-validated functional commit:

`5c4d1bbc72ee24c84b21acdbff612652ab6be57f`

The state matrix marks both ordered pairs as `recovered_exact=yes`.
