# `fa_game_info` `0x18644`: exact `0x106 ↔ 0x116` recovery (v0045)

The ordered pair family `0x106 ↔ 0x116` is now ROM-backed and exact for the recovered native child at `0x00018644`.

States:

- `0x106 = bit8 | bit1 | bit2`
- `0x116 = bit8 | bit1 | bit2 | bit4`
- vector class: 6 (`219,224,216,221`)

Semantic recovery admits only this exact bilateral composition in the general state gate and the bit1-specific gate. The threshold block required no change because this corridor remains on the shared bit1 path.

With only the semantic admission applied, the architectural snapshot already matched ROM for every tested orientation/countdown/mode combination. The remaining differences were instruction accounting only.

Measured native-minus-ROM deltas before accounting:

- `0x106 -> 0x116`, first call (`0x164b0`): `-4` without countdown, `-15` with countdown;
- `0x106 -> 0x116`, second call (`0x164c4`): `+3/+2` without countdown for mode6 `0/1`, `-14` with countdown;
- `0x116 -> 0x106`, first call (`0x164b0`): `-1` without countdown, `-15` with countdown;
- `0x116 -> 0x106`, second call (`0x164c4`): `0/-1` without countdown for mode6 `0/1`, `-14` with countdown.

The caller presents the fighter pair in reversed `r7/r8` order at the second invocation, so the final accounting intentionally applies the second-call corrections to the opposite forward/reverse predicate from the first call.

Validation against the supplied VF2 v2.2 ROM set:

- isolated first/second-call differential: 16/16 exact;
- chained `C child #1 -> caller ROM -> C child #2 -> tail ROM`: 8/8 exact;
- architectural snapshot: exact;
- instructions, calls, returns, interrupt entries and interrupt returns: all zero delta.

ROM-validated functional candidate: `a17cd01d464798e279233f7cdeaf44f005f9a886`.

After this recovery the enumerated matrix has 34/64 ordered pairs individually marked exact, with all nine instruction-vector classes still represented.
