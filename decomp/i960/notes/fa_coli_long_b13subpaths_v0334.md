# v0334 (measurement): coli bit-13 sub-paths

## Measured

| Shape | Probe | Path |
|-------|-------|------|
| bit 13 + bit 3 set | **12** | early-exit `0x230a0` (already native, v0323) |
| bit 13 + scan 2 | **310** | `0x22738` → `+0x828` bit 12 clear → warm cascade |
| bit 13 + `+0x828` bit 9 | **317** | `0x2272c` → bit 12 clear → warm cascade |

## Code status

Scan 2/5/6 and `+0x828` bit 9 clear → warm cascade paths are
implemented in `coli_225cc_long_body`. Unit-test shapes for these
sub-paths fail due to state interactions with the preceding
bit-13-profundo shape (counter/diagnostic mutations). The shapes
need a dedicated setup or a separate test function.

Bit 13 + bit 3 set is already handled by the v0323 early-exit
(scan=0 gate fires first).

## Still fail-closed

- bit 13 + bit 3 set with scan ≠ 0 → `0x22744`
- `+0x828` bit 12 set → `0x22794`

## Drive

```text
vf2probe --snapshot out/coli-225cc-entry.vf2snap \
  --set-u8 0x005111a2=1 --set-u32 0x0050a0b4=2 \
  --set-u32 0x00512b24=0x2000 \
  [--set-u8 0x005111a1=2 | --set-u16 0x005111a8=0x200] \
  --until 0x000230b8 --max-steps 800
```

## Proof

- unit `test_coli_225cc_long`: all existing shapes pass (bit-13
  profundo 223 unchanged)
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug **56/56**
