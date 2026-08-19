# game_info 0x18644 — exact 0x110 ↔ 0x114 recovery (v0041)

The ordered state pair `0x110 ↔ 0x114` (`state8+bit4` versus `state8+bit2+bit4`) is now ROM-backed in both orientations.

## ROM vector

The pair belongs to vector class 2: `(202, 207, 202, 207)` for `(cd0/m0, cd0/m1, cd1/m0, cd1/m1)`.

## Validation

The semantic-only candidate matched CPU/mutable-memory state and procedure/interrupt counters in all 16 isolated child probes. The only mismatch was instruction accounting.

The measured accounting is symmetric in both orientations:

- first call (`0x164b0`): countdown clear `-6`, countdown set `+8`;
- second call (`0x164c4`): countdown set `+7`; countdown clear `-10` with mode bit 6 clear and `-9` with mode bit 6 set.

Final validation:

- 16/16 isolated child probes exact;
- 8/8 chained `C child #1 -> caller ROM -> C child #2 -> tail ROM` probes exact;
- CPU/mutable memory, executed instructions, procedure calls/returns, interrupt entries/returns all delta zero.

Validated functional commit: `35e25f03ade7754fb33d10045763f384fee7647e`.

The admission remains exact to `0x110 ↔ 0x114`; no wider bit2/bit4 family is inferred solely from this result.
