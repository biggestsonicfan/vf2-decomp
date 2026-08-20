# `fa_game_info` `0x18644`: exact `0x116 ↔ 0x116` recovery (v0047)

The symmetric ordered-pair corridor `0x116 ↔ 0x116` is now ROM-backed and exact for the recovered native child at `0x00018644`.

State:

- `0x116 = bit8 | bit1 | bit2 | bit4`
- vector class: 8 (`224,229,224,229`)

Semantic recovery admits only the exact bilateral `0x116/0x116` composition in the general bilateral gate and the bit1-specific gate.

Before instruction accounting, native execution already matched ROM architecturally in every measured case. The measured native-minus-ROM instruction deltas were:

- first call (`0x164b0`): `-5` with countdown clear, `-19` with countdown set;
- second call (`0x164c4`): `-1/-2` with countdown clear for mode6 `0/1`, `-18` with countdown set.

The corresponding positive correction is applied only to the exact `0x116/0x116` composition.

Validation against ROM:

- isolated child/call-site differential: 8/8 exact (two call sites × four countdown/mode combinations);
- chained `C child #1 → caller ROM → C child #2 → tail ROM`: 4/4 exact;
- architectural snapshot: exact;
- instructions, calls, returns, interrupt entries and interrupt returns: all zero delta.

ROM-validated functional candidate: `6b66e4f18a5b1f84fea31ae2296e5e5713d9eb65`.

After this recovery the enumerated matrix has 36/64 ordered pairs individually marked exact, with all 9 instruction-vector classes represented.
