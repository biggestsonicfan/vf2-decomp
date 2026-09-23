# coli campaign final status (v0330–v0338)

## Closed (native)

| Slice | Item | Unit/Probe |
|-------|------|------------|
| v0330 | bit 26 (join `0x22e24` + `0x230d4` compacto) | 263 |
| v0331 | r11≥40 | 300 |
| v0332 | bit 4 (4 sites) | 301 |
| v0333 | bit-13 profundo (alt tail `0x22808`) | 223 |
| v0334 | scan 2/5/6, +0x828 bit 9 → cascade | 310/317 |
| v0335 | `0x22794` scale (r11>>=1, r9*=0.5) | 263 |
| v0336 | `0x22744`/`0x22778`/`0x227ac` (bit 13+bit 3) | 328 |
| v0337 | `0x227c4` (+0x828 bit 13 → alt tail) | 219 |
| v0338 | `g7+0x1a4` bit 22 + scan=3 witness | 305/301 |

## Still fail-closed

| Item | Reason |
|------|--------|
| `0x22d8c` | g7 bit 22 + g8 bit 11 → `0x230d4` g0=5 (unimplemented) |
| `0x227dc` | needs type-5 record in table (unreachable) |
| `0x502a4` | board bit 9 clear on bit-14 (unsupported instruction) |
| Unit shapes | sub-path shapes need dedicated test setup |

## Pins

All slices: PUNCH **320/320 MATCH**, input-17 **64/64 MATCH**,
ctest Debug **56/56**.
