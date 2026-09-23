# v0338: coli g7+0x1a4 bit 22 + scan≠0/4 witness

## Verdict

`g7+0x1a4` bit 22 set is native at all four `0x22e24` join sites:
`bbc 22 not taken` → check `g8+0x1a4` bit 4 (taken → join) or
bit 11 (set → fail-closed `0x22d8c`, clear → join). Probe **305**.

`g7+0x821=3` (scan not 0/4) already works: the `cmpibne 0` at
`0x225f8` is taken, skipping the `bbs 3` early-exit. Probe **301**
(warm 302 − 1). No code change needed — the UNCOVERED note was
outdated.

Board bit 9 clear on bit-14 hits `call 0x502a4` (unsupported
instruction) — correctly fail-closed.

## Proof

- unit `test_coli_225cc_long`: all existing shapes pass
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug **56/56**

## Still fail-closed

- `0x22d8c` (g7 bit 22 + g8 bit 11 set)
- `0x227dc` (needs type-5 record)
- `0x502a4` (board bit 9 clear on bit-14)
- Unit-test shapes for the new sub-paths
