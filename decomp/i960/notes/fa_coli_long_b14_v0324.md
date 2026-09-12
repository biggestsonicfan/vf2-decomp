# v0324: coli long-body g8+0x1a4 bit 14 sibling

## Verdict

`g8+0x1a4` bit 14 is native on the long body. Two sites:

1. **Cascade `0x22b44`**: `bbs 14 taken` → `0x22b6c` (skip the
   `0xd9b0` mask check). Warm does `bbs nt` + mask (4 insns);
   bit14 does `bbs taken` (1 insn).
2. **Post-diagnostic `0x22c8c`**: `bbs 14 taken` → `0x22dd4`
   (`g8+0x6d9++`, board bit 9 taken → `0x22e24`). Skips the
   `g7+0x828` cascade.

Unit shape **289** with `g8+0x6d9` ending at 1 and
`0x50a16e >= 1` (float-tail threshold).

## Drive

```text
vf2probe --snapshot out/coli-225cc-entry.vf2snap \
  --set-u8 0x005111a2=1 --set-u32 0x0050a0b4=2 \
  --set-u32 0x00512b24=0x4000 \
  --until 0x000230b8 --max-steps 500
```

Probe measured 289. Unit 289 including the completed ret.

## Proof

- unit `test_coli_225cc_long`: bit14 289, `d6d9==1`
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
- ctest Debug **56/56**

## Open

- bit 4 / bit 26 at `0x22c8c`
- `g7+0x828` bit 14 (293)
- `r11 >= 40` / `cmpoble 30`
