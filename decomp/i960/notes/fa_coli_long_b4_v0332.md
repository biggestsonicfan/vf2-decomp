# v0332: coli long-body g8+0x1a4 bit 4 (4 sites)

## Verdict

`g8+0x1a4` bit 4 set is native on the long body. Four sites:

1. **Cascade `0x22b7c`**: `flags & 0x4010 == 16` → `r11 = r11*3>>1`,
   `g0 = 0x23d6b`, skip `g8+0x804`, join at `0x22bc0`.
2. **Post-diag `0x22c98`**: `bbs 4 taken` → `0x22e24` join (skips
   the `g7+0x828` cascade).
3. **After `0x230d4` `0x22e44`**: `bbc 4 not taken` → `g0 = 0x2ce`
   before the second `0x23238` (takes the long float path).
4. **Miss tail `0x22ee0`**: `bbs 4 taken` → offsets `0x1c`/`0x10`
   instead of `0x18`/`0x8`.

Unit shape **301**, `g8+0x198 = 0x0c0002cf` (g0 from the `0x23238`
long path with `field_1f8 <= 0.6`).

## Drive

```text
vf2probe --snapshot out/coli-225cc-entry.vf2snap \
  --set-u8 0x005111a2=1 --set-u32 0x0050a0b4=2 \
  --set-u32 0x00512b24=0x10 \
  --until 0x000230b8 --max-steps 800
```

Probe measured **301**. Unit 301 including the completed ret.

## Unit-test notes

- `0x1ab34` index `0x2cf` (from `g0=0x2cf`) needs a two-iteration
  type-3/8 miss record.
- Miss-tail offsets `0x50a810` / `0x50a81c` planted (r7 base
  `0x50a800` when `g7+0x843 = 0`).
- Diagnostic slot `0x504078` cleared between shapes.

## Proof

- unit `test_coli_225cc_long`: bit4 **301**, `g8+0x198 = 0x0c0002cf`
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug **56/56**

## Open

- bit-13 profundo (`+0x5b8` bit 0 clear, bit 3 set, scan 2/5/6)
- `g7+0x821 != 0` (except 0 and 4), board bit 9 clear on bit-14,
  `g7+0x1a4` bit 22 set — no witnesses
