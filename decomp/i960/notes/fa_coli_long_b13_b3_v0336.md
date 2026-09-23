# v0336: coli bit-13 + bit 3 sub-paths (0x22744/0x22778/0x227ac)

## Verdict

`g8+0x1a4` bit 13 + bit 3 set paths are native:

| Path | Condition | Action |
|------|-----------|--------|
| `0x227dc` | `g7+0x844` bit 30 set | fail-closed (needs type-5 record, unreachable) |
| `0x22764` | scan ∈ {2,5,6} | shared scan path (bit 12 → scale or cascade) |
| `0x22778` | scan ∉ {2,5,6} | `+0x828` bit 13 → fail-closed; bit 12 → `0x22794` scale; scan==1 → `0x227ac` scale; else cascade |
| `0x227ac` | scan == 1 | `r11 = r11*3/4`, `r9 *= 0.75f`, cascade |
| `0x22794` | `+0x828` bit 12 set | `r11>>=1`, `r9*=0.5`, `r8=1`, cascade at `0x22918` |

## Probes

- scan 1, +0x828=0: **328** instructions
- scan 2, +0x828 bit 9 clear: **224** (same alt tail as profundo)

## Proof

- unit `test_coli_225cc_long`: all existing shapes pass
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug **56/56**

## Still fail-closed

- `0x227c4` (`+0x828` bit 13 set on `0x22778` path)
- `0x227dc` (needs type-5 record in table)
- Unit-test shapes for the new sub-paths
