# v0317: coli `0x18bd4` notbit-15 sibling

## Verdict

`g8+0x19c` bit 15 set no longer fails closed. After the type-5 walk,
`notbit 15` flips bit 15 of the halfword at `walk+1` before the
`(17<<24) + r3` pack into `g7+0x198`. Adds **1** instruction.

Measured reference: 69 → **70** steps from `0x225cc` to `0x230b8`.

Unit shape: type22 first-hit + bit 15 → **54/3/4**;
`g7+0x198 = (17<<24) + 0x8123` when the raw halfword is `0x0123`.

## Proof

- unit test in `test_coli_225cc_type22`
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
