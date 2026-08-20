# fa_game_info 0x18644: exact 0x112 ↔ 0x116 recovery (v0043)

The ordered pair family `0x112 ↔ 0x116` is now ROM-backed and exact for the recovered native child at `0x00018644`.

States:

- `0x112 = bit8 | bit1 | bit4`
- `0x116 = bit8 | bit1 | bit2 | bit4`
- vector class: 8 (`224,229,224,229`)

Semantic recovery admitted only this exact bilateral composition in the general state gate and the bit1-specific gate. The threshold block required no change because this corridor remains on `shared_bit1_path`.

Measured native-minus-ROM instruction deltas before accounting were symmetric in both orientations:

- first call (`0x164b0`): `-5` without countdown, `-19` with countdown;
- second call (`0x164c4`): `-1/-2` without countdown for mode6 `0/1`, `-18` with countdown.

The final accounting applies the corresponding positive corrections only to `0x112 ↔ 0x116`.

Validation against ROM:

- isolated call-site differential: 16/16 exact;
- chained `C child #1 → caller ROM → C child #2 → tail ROM`: 8/8 exact;
- architectural snapshot: exact;
- instructions, calls, returns, interrupt entries and interrupt returns: all zero delta.

ROM-validated functional candidate: `8de1c59c7490146f0930edd53e9b912391dd75b5`.

After this recovery the enumerated matrix has 30/64 ordered pairs individually marked exact, with all 9 instruction-vector classes still represented.
