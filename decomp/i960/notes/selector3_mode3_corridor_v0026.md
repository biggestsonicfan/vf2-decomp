# Selector 3 mode-3 corridor

The selector-3 bridge introduced while extending the second dispatch is now tied back to concrete VF2 2.1 i960 code instead of being treated only as a post-frame state snapshot.

The exact ROM image supplied for this recovery reconstructs the following call chain:

```text
0x0001fe74 -> 0x0004b410
0x0001fed8 -> 0x0002eab8
0x0001fedc -> 0x00011704
```

A second caller reaches `0x0004b410` at `0x0000ce48`.

## 0x0004b410 — video command submit

The function is a compact eight-instruction leaf:

```text
0x0004b410  mov   1, g3
0x0004b414  st    g3, 0x00550000
0x0004b41c  lda   0x005502e0, g3
0x0004b424  mov   3, g15
0x0004b428  st    g15, 0(g3)
0x0004b430  st    r0, 4(g3)
0x0004b438  st    r1, 8(g3)
0x0004b440  st    r2, 12(g3)
0x0004b448  ret
```

This establishes the command tuple written by the selector-3 state bridge as real program behavior: selector `3` is committed at `0x005502e0`, while the three arguments arrive in `r0`, `r1`, and `r2`. In the observed mode-3 path those arguments are `0x21`, `0x0a`, and `0x7c`.

## 0x0002c38 — color-table rebuild

`0x0002c38` is also reached by direct calls (`0x0002004c` and `0x00047730`). It clears the command/color work area beginning at `0x00546008`, derives three channels from the six bytes at `0x00500234..0x00500239`, scales them by the three live bytes `0x005000e0..0x005000e2`, writes 16-bit channel triples, then finishes with:

```text
0x00002dc4  mov   0, g15
0x00002dc8  st    g15, 0x00546004
0x00002dd0  mov   1, g15
0x00002dd4  st    g15, 0x00546000
0x00002de0  ret
```

That directly explains the selector-3 bridge's `0x00546000 = 1` / `0x00546004 = 0` state.

## 0x00011704 — 128-byte row/table copier

The leaf at `0x00011704` copies 128 bytes per outer iteration from the table at `0x00078d10` to the destination at `0x12800000`, advancing the destination by four bytes per source byte. Its outer count is loaded from `0x00078d0c`.

This function is not folded into a guessed high-level rendering primitive yet. The recovered name remains deliberately structural until a differential checkpoint proves the consumer semantics.

## Recovery boundary

The current selector-3 bridge still represents the complete observed phase transition atomically, but these three ROM-backed anchors remove important portions from the unknown set. Future work should split the 123,638-instruction selector-3 delta at these real procedure boundaries rather than adding more synthetic state.
