# game_info 0x18644 — exact 0x100 ↔ 0x114 recovery (v0040)

The ordered state pair `0x100 ↔ 0x114` (`state8` versus `state8+bit2+bit4`) is now ROM-backed in both orientations.

## ROM vector

The pair belongs to vector class 1: `(197, 202, 194, 199)` for `(cd0/m0, cd0/m1, cd1/m0, cd1/m1)`.

## Validation

The semantic-only candidate matched CPU/mutable-memory state and procedure/interrupt counters in all 16 isolated child probes. The remaining differences were instruction accounting only.

Measured accounting used the proven second-call local-role inversion at return address `0x164c4`.

Final validation:

- 16/16 isolated child probes exact;
- 8/8 chained `C child #1 -> caller ROM -> C child #2 -> tail ROM` probes exact;
- executed instructions, procedure calls/returns, interrupt entries/returns all delta zero.

Validated functional commit: `6fbeaf978af61bb2089e0147b07fef371059a330`.

## Structural observation

The final accounting table is identical to the already recovered `0x104 ↔ 0x110` (`state8+bit2` versus `state8+bit4`) corridor. This is evidence for a possible structural bit2/bit4 family, but no broader state family is admitted solely from this equality; unrecovered pairs remain fail-closed until separately validated.
