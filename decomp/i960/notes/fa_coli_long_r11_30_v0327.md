# v0327: coli long-body r11>=30 cmpoble gate

## Verdict

`cmpoble 30, r11` taken joins at `0x22e24` (skip bit 8, bit 24 and
the `+0x1ac` compare). Unit **299** with r11=30.

## Proof

- unit `test_coli_225cc_long`: r11=30 299
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug **56/56**

## Open

- `r11 >= 40` (also sets r4=8 in the diagnostic arm)
- bit 4 / bit 26 at `0x22c8c`
- `g7+0x828` bits 10 / 8
