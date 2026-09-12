# v0333: coli long-body bit-13 profundo (g8+0x5b8 bit 0 clear)

## Verdict

`g8+0x1a4` bit 13 set with `g8+0x5b8` bit 0 clear takes an alternate
tail at `0x22808` when bit 3 is clear, scan-byte ∉ {2,5,6}, and
`g7+0x828` bit 9 is clear. The alt tail:

1. counter-- at `g7+0x1234`, counter++ at `g7+0x1238`
2. diagnostic pair with `g0 = 0x9e167f`
3. `g0=0`, `r8=0`, store `0x14000002` at `g7+0x194`
4. call `0x230d4` with `g0=0` (long path, r3=2)
5. store packed at `g8+0x198` (`shlo 25,7`)
6. copy `g7+0x6b2` → `g8+0x6bc`
7. optional `g7+0x1224` bit-1 copy
8. `g7+0x85c` / ROM `0x23282` / lim `0x50a14c` scale
9. stos at `g8+0x5de`, stob at `0x50a0b6`
10. jump to float tail `0x22fbc`

Unit shape **223** (probe 222 + ret), `g8+0x198 = 0x0e0000ee`.

## Sub-paths still fail-closed

- bit 3 set → `0x22744`
- scan-byte ∈ {2,5,6} → `0x22738`
- `g7+0x828` bit 9 set → `0x22794` / `0x22914`

## Drive

```text
vf2probe --snapshot out/coli-225cc-entry.vf2snap \
  --set-u8 0x005111a2=1 --set-u32 0x0050a0b4=2 \
  --set-u32 0x00512b24=0x2000 \
  --until 0x000230b8 --max-steps 800
```

## Unit-test notes

- Needs `0x230d4` table slot at main-data `0x1cd7c = 0xee` (r3=2
  when g0_in=0 and r9=2).
- Shape placed at the end of `test_coli_225cc_long` to avoid
  mutating counter/diagnostic state used by earlier shapes.

## Proof

- unit `test_coli_225cc_long`: b13profundo **223**, `g8+0x198 = 0x0e0000ee`
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug **56/56**

## Open

- bit-13 sub-paths (bit 3, scan 2/5/6, +0x828 bit 9)
- `g7+0x821 != 0` (except 0 and 4), board bit 9 clear on bit-14,
  `g7+0x1a4` bit 22 set — no witnesses
