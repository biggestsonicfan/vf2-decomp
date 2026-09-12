# v0323: coli long-body early exit at 0x230a0

## Verdict

`g8+0x1a4` bit 3 / bit 15 / bit 16 now take the measured `0x230a0`
early exit (counter-- at `g7+0x1234`). Inserted **before** the
existing warm fail-closed so the `+2 +4` accounting is unchanged.

| shape | body | unit total |
|-------|------|------------|
| warm (bits 3/15/16 clear) | 6 (unchanged) | 249/302 etc. |
| bit 16 set, `+0x821==0` | 10 | 16 |
| bit 15 set, `+0x821==0` | 9 | — |
| bit 15 set, `+0x821!=0,!=4` | 8 | — |
| bit 3 set, `+0x821==0` | 7 | — |

`bbs 15` is tested before `bbc 16` (`0x22600` → `0x22608`). Bit 15
with `+0x821==4` falls through to the warm path (still fail-closed
for unmeasured combos).

Net counter: caller prefix `++` then early-exit `--` leaves
`g7+0x1234` unchanged.

## Why v0322 failed

A full flags-region restructure replaced the `+2 +4` block and
shifted every warm body count. v0323 only inserts early-exit returns
before the existing block; the warm path is byte-identical.

## Proof

- unit `test_coli_225cc_long`: all prior shapes unchanged + bit16=16
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug **56/56**

## Open

- bit 14 / bit 4 at `0x22c84` (blocked by an earlier `flags_g8` bit-14
  check at ~`0x226xx`)
- `g7+0x828` bit 14 (293)
- `r11 >= 40` / `cmpoble 30`
