# fa_game_info 0x18644: exact 0x114 ↔ 0x116 recovery (v0049)

## Corridor

- `0x114 = bit8 | bit2 | bit4`
- `0x116 = bit8 | bit1 | bit2 | bit4`
- vector class: `5`
- ROM vector: `(213, 218, 213, 218)`

## Semantic probe

After admitting only the bilateral `0x114 ↔ 0x116` composition, every isolated case matched architecturally. The remaining differences were instruction accounting only.

Measured semantic-only deltas, candidate minus ROM:

### `0x114 -> 0x116`

- first call (`0x164b0`): `-5` with countdown 0, `-19` with countdown 1
- second call (`0x164c4`): `+10/+9` for countdown 0 (`mode6=0/1`), `-7` with countdown 1

### `0x116 -> 0x114`

- first call (`0x164b0`): `+6` with countdown 0, `-8` with countdown 1
- second call (`0x164c4`): `-1/-2` for countdown 0 (`mode6=0/1`), `-18` with countdown 1

The required corrections are the existing class-5 accounting patterns, with the caller's second invocation presenting the pair in reversed `r7/r8` order.

## Validation

With the measured accounting applied:

- isolated child checkpoints: **16/16 exact**
- chained native child #1 -> caller ROM -> native child #2 -> tail ROM: **8/8 exact**
- CPU/register architecture: exact
- memory: exact
- instruction/call/return/interrupt counters: zero delta in every tested case
- probe CI build and `ctest`: success

Clean source candidate: `4f185084ec98334c3d9c0158412ef8d6ba4cf550`.

After promotion, the state-pair matrix contains **40/64 exact pairs**.
