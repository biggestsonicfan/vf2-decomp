# v0318b/c: coli long-body bit-4-only and bbs-15-taken siblings

## Verdict

Two more `g7+0x1a4` bit-4 siblings are native:

| Shape | Δ vs all-clear | Total |
|-------|----------------|-------|
| bit 4 set, bit 12 clear | +2 | **251** |
| bits 4+12, `bbs 15` taken | +7 (skip ×0.5 scale) | **256** |

Also fixed the `subi` operand order in the bits 4+12 packed value:
`r10 - half_c + (1<<14)`, matching `subi r3,r10,r3`.

## Drive

- bit-4-only: `g7+0x1a4 = 0x10010`
- bbs15 taken: `g7+0x1a4 = 0x11010`, `g7+0x5c2 = 0xC000`
  (`packed = 0 - (-0x4000) + 0x4000 = 0x8000`)

## Proof

- unit tests in `test_coli_225cc_long`: 251 and 256
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
