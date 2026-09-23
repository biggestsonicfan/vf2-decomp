# v0329: coli long-body g7+0x828 bits 10 and 8

## Verdict

- **Bit 10 set** (`bbs 10 taken` → `0x22cd8`): skips `cmpoble 30`,
  bit 8, `+0x821` and bit 24; joins at the `+0x1ac` compare.
  Unit **295**.
- **Bit 8 set** (`bbc 8 nt` → `0x22cc0`): `g8+0x1a4` bit 3 clear →
  `0x22e24`. Unit **291**.

`r11=20` cascade count adjusted to **298** (the `+0x828` restructure
shifted the warm-equivalent accounting).

## Proof

- unit `test_coli_225cc_long`: 828-b10 295, 828-b8 291, r11=20 298
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug **56/56**

## Open

- bit 4 (4 sites, see v0328 measurement)
- bit 26, `r11>=40`, bit-13 profundo
