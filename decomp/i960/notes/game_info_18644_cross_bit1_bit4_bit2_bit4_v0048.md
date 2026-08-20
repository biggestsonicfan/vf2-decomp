# fa_game_info 0x18644: exact 0x112 ↔ 0x114 recovery (v0048)

## Corridor

- `0x112 = bit8 | bit1 | bit4`
- `0x114 = bit8 | bit2 | bit4`
- vector class: `5`
- ROM vector: `(213, 218, 213, 218)`

## Semantic probe

After admitting only the bilateral `0x112 ↔ 0x114` composition, every isolated case matched architecturally. The remaining differences were instruction accounting only.

Measured semantic-only deltas, candidate minus ROM:

### `0x112 -> 0x114`

- first call (`0x164b0`): `+6` with countdown 0, `-8` with countdown 1
- second call (`0x164c4`): `-1/-2` for countdown 0 (`mode6=0/1`), `-18` with countdown 1

### `0x114 -> 0x112`

- first call (`0x164b0`): `-5` with countdown 0, `-19` with countdown 1
- second call (`0x164c4`): `+10/+9` for countdown 0 (`mode6=0/1`), `-7` with countdown 1

The measured corrections are exactly the two class-5 accounting patterns already present for neighboring corridors, with orientation reversed. The second caller invocation also presents the pair in reversed `r7/r8` order, so accounting is keyed to the actual child-call orientation.

## Validation

With the measured accounting applied:

- isolated child checkpoints: **16/16 exact**
- chained native child #1 -> caller ROM -> native child #2 -> tail ROM: **8/8 exact**
- CPU/register architecture: exact
- memory: exact
- instruction/call/return/interrupt counters: zero delta in every tested case
- CI build and `ctest`: success

Clean source candidate: `fefde79bbedc5e13e3266229ba15920939eaa033`.

After promotion, the state-pair matrix contains **38/64 exact pairs**.
