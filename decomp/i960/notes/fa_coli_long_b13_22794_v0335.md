# v0335: coli bit-13 sub-path 0x22794 (scale transform)

## Verdict

`+0x828` bit 12 set on the bit-13 path (`0x22794`) is native:
`r11 >>= 1`, `r9 *= 0.5f`, `r8 = 1`, enter cascade at `0x22918`
(skipping `mov 2, r8`).

Measured probe: **263** instructions. Reached from both the scan
2/5/6 path and the `+0x828` bit 9 set path.

## Code

`cascade_r8_set` flag skips the cascade's `mov 2, r8` when the
scale transform already set r8=1.

## Still fail-closed

- `0x22744` (bit 13 + bit 3 + scan≠{2,5,6} → `0x22778`)
- `0x227dc` (bit 13 + bit 3 + `g7+0x844` bit 30 set)
- `0x22794` unit-test shapes (need dedicated setup)

## Proof

- unit `test_coli_225cc_long`: all existing shapes pass
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug **56/56**
