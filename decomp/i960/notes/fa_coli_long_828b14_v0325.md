# v0325: coli long-body g7+0x828 bit 14 sibling

## Verdict

`g7+0x828` bit 14 set joins at `0x22e24` (skip the `+0x828` cascade:
bit 10, `cmpoble 30`, bit 8, `+0x1ac` compare). Unit **286**.

## Proof

- unit `test_coli_225cc_long`: 828-bit14 286
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug **56/56**

## Open

- `g7+0x828` bits 10 and 8 set
- `r11 >= 40` / `cmpoble 30`
- bit 4 / bit 26 at `0x22c8c`
