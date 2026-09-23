# v0318: coli long-body `g7+0x1a4` bits 4+12 scale sibling

## Verdict

When `g7+0x1a4` bit 4 is set (previously fail-closed), the measured
sibling requires bit 12 set as well, then:

```text
ldis 0x5c2(g7); subi r10; shlo 14,1; addi
bbs 15 → cascade (unmeasured if taken)
lda 0.5f; mulr g7+0x2c; st; mulr g7+0x34; st
→ fall through to cascade
```

Adds **14** instructions over the bit-4-clear path (249 → **263**
including ret on the v0288 drive).

Bit 4 set with bit 12 clear, and `bbs 15` taken, remain fail-closed.

## Drive

`out/coli-225cc-entry.vf2snap` with `g7+0x1a4 = 0x11010`
(bit 16 + 4 + 12), `g7+0x5c2 = 0` → packed `0x4000`, bit 15 clear.

## Proof

- unit test in `test_coli_225cc_long`: 263; `+0x2c` 2.0f→1.0f,
  `+0x34` 4.0f→2.0f
- PUNCH **320/320 MATCH**
- input-17 **64/64 MATCH**
