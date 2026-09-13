# v0337: coli 0x227c4 (+0x828 bit 13 on 0x22778 path)

## Verdict

`+0x828` bit 13 set on the `0x22778` path is native: diagnostic
pair with `g0=0x9e167f`, then `g0=1`, join the bit-13-profundo
alt tail at `0x22848`. Probe **219**.

## Code

`bit13_alt_join` label shares the alt tail between the profundo
path (g0=0) and `0x227c4` (g0=1).

## Proof

- unit `test_coli_225cc_long`: all existing shapes pass
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug **56/56**

## Still fail-closed

- `0x227dc` (needs type-5 record in table — unreachable)
- Unit-test shapes for the bit-13 sub-paths
- `g7+0x821 != 0` (except 0 and 4), board bit 9 clear on bit-14,
  `g7+0x1a4` bit 22 set — no witnesses
