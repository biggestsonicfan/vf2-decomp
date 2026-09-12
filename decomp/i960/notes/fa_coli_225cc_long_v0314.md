# v0314: coli long body `0x225cc` (248-step measured path)

## Verdict

`coli_225cc_body` now admits the v0288 long drive shape (bit 3 clear).
`vf2_hybrid_coli_225cc_execute` completes **249** instructions
(248 reference steps to `0x230b8` plus the completed `ret`), **4**
nested calls, **5** returns, `g0 = 0x0c0004ac` at `g8+0x198`.

The compact bit-3 sibling (body 12) is unchanged. Unmeasured flag
siblings, `g8+0x19f == 22`, and the `0x2227c` tie-break remain
fail-closed.

## Regions (reference path `out/v314/225cc-path.txt`)

| Range | Role | Native body |
|-------|------|-------------|
| `0x225cc..0x225f0` | counter++, type ≠ 22 | 5 (caller) |
| `0x225f0..0x22914` | flags + scanbit float pack | 42 |
| `0x22914..0x22e3c` | cascade all-clear | 60 |
| `0x22e3c` | call `0x230d4` long | 1 + child 43 + ret 1 |
| `0x22e40..0x22e90` | call `0x23238` + stores | 21 |
| `0x22e90` | call `0x1ab34` miss | 1 + child 16 + ret 1 |
| `0x22e94..0x22fbc` | miss tail | 18 |
| `0x22fbc..0x230b8` | float tail + FIFO | 40 |

## Measured facts

- `be` at `0x22624` is **not** taken: `cmpibl`/`cmpob*` do not publish
  `compare_result`, so `be` tests stale CC (NONE after reset).
- Scanbit pack of `r9 == 0` → bit 31, `r14 = 158`, packed
  `0xcf000000`; `mulr 0x3d088888` → `0xcc888888`.
- `r11` stays the `g7+0x822` byte (0), **not** the float scale (`r9`).
- `0x230d4` long path (bit 26 clear) is native inside the parent via
  body-only `coli_230d4_long_body` (parent 40 + `0x23238` early-out 2
  + ret 1 = 43).
- `0x1ab34` walks type 3 then type 8 at table index `g0 & 0x1fff`
  (`0x4ac`), miss `g0 = 0`, body 16.
- FIFO `0x884000` is a last-write buffer without TGP callbacks
  (write cmd, write data, read returns data).

## Drive / proof

- trace: `out/v314/225cc-long.jsonl`, path `out/v314/225cc-path.txt`
- unit test `test_coli_225cc_long`: 249/4/5, `g8+0x198 = 0x0c0004ac`,
  cursor halfword `0xfff2`, `g8+0x6d8 = 1`, `g8+0x700` bit 1 set
- unit test `test_coli_230d4_bit26`: compact 16/0/1 unchanged; long
  44/1/2 `g0 = 0x4ac`
- PUNCH **320/320 MATCH** / 12,946 blocks / 14,962,620 insns
- input-17 **64/64 MATCH** / 2,368 blocks / 2,428,988 insns
- ctest Debug **56/56**
- `vf2_native_runtime_tests` passed

## Open siblings

- `g8+0x1a4` bits 3/15/16/13/4/26/25/14/22 set paths
- `g7+0x1a4` bit 4 / bit 12 set paths
- `g8+0x19f == 22` shortcut (`0x18bd4`)
- `0x2227c` double-call tie-break
- `be` taken sibling (skip pack, `r9 = 0`) unmeasured as a long-body
  entry
- `r11b != 0` / `r11b < 0` flag-scale variants
