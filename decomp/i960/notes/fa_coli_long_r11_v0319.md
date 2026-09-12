# v0319: coli long-body `r11 != 0` packing + diagnostic arm

## Verdict

`g7+0x822` (`r11`) non-zero is no longer fail-closed on the measured
shape:

1. **Packing** at `0x22640`: `scanbit(r11)` → clrbit/shlo/lda/shlo/or
   (8 insns instead of the `r11==0` 12).
2. **Cascade diagnostic arm** when `cmpobe 0,r11` is not taken:
   `g7+0x1a4` bit 18 clear, `g7+0x823 == 0`, `r11 < 20`, then
   `0x439ac` (store/count, body 13) and `0x43888` (body 26), join at
   `0x22c84`.

Unit shape `r11b = 1` with `0x50a0b4 >= r11` and ROM `0x230c8`
non-zero: **302 / 7 / 8**.

## Open

- `0x439ac` multi-trip loop / count > 4
- `0x43888` branch-byte arm (`0x50002c & 12 != 0`) and runtime bit 20
- `g7+0x1a4` bit 18 set, `g7+0x823 != 0`, `r11 >= 20`

## Proof

- unit test in `test_coli_225cc_long`: 302
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
