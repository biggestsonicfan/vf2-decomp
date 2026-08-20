# `fa_game_info` `0x18644`: exact `0x106 ↔ 0x106` recovery (v0046)

The symmetric ordered-pair corridor `0x106 ↔ 0x106` is now ROM-backed and exact for the recovered native child at `0x00018644`.

State:

- `0x106 = bit8 | bit1 | bit2`
- vector class: 9 (`232,237,208,213`)

Semantic recovery admits only the exact bilateral `0x106/0x106` composition in the general bilateral gate and the bit1-specific gate.

Before instruction accounting, native execution already matched ROM architecturally in every measured case. The measured native-minus-ROM instruction deltas were:

- first call (`0x164b0`): `-9` with countdown clear, `-11` with countdown set;
- second call (`0x164c4`): `-5/-6` with countdown clear for mode6 `0/1`, `-10` with countdown set.

The corresponding positive correction is applied only to the exact `0x106/0x106` composition.

Validation against ROM:

- isolated child/call-site differential: 8/8 exact (two call sites × four countdown/mode combinations);
- chained `C child #1 → caller ROM → C child #2 → tail ROM`: 4/4 exact;
- architectural snapshot: exact;
- instructions, calls, returns, interrupt entries and interrupt returns: all zero delta.

ROM-validated functional candidate: `b1c46b9df3b3bda6cebd4b64526ae3862b980329`.

After this recovery the enumerated matrix has 35/64 ordered pairs individually marked exact, with all 9 instruction-vector classes represented.
