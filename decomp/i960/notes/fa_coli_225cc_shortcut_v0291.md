# fa_coli `0x225cc` `g8+0x19f==22` shortcut — v0291

## Summary

Measurement-only. Forces the previously unmeasured `0x18bd4` shortcut
inside `0x225cc` by setting `g8+0x19f = 22`. Decides whether a compact
leaf is recoverable. **Verdict: DEFER.**

## Drive

From `out/coli-225cc-entry.vf2snap` (parked at `0x225cc`):

```text
vf2probe --snapshot out/coli-225cc-entry.vf2snap \
  --set-u8 0x00512b1f=22 --until 0x000230b8 --max-steps 100 --trace
```

`g8 = 0x512980` (measured from the `ldob 0x19f(g8)` memory trace;
`0x512b1f = g8+0x19f`).

## Measured path

```text
0x225cc  ld/lda/st     counter++ at g7+0x1234
0x225e0  ldob 0x19f(g8), r14
0x225e4  cmpobne 22    not taken
0x225e8  call 0x18bd4
0x225ec  b 0x230b8
```

`0x18bd4` is **not** a compact leaf:

```text
0x18bd4  ldos 0x19c(g8), r4
0x18bd8  mov r4, g0
0x18bdc  mov 5, g1
0x18be0  call 0x1ab34      # table walk over ROM 0x0200d34c
0x18be4  ldos 0x1(g0), r3
0x18be8  bbc 15, r4 ...
         ... notbit/shlo/addi/st ...
0x18c04  call 0x18b58      # another helper
         ...
```

`0x1ab34` indexes ROM via `g0 & 0x1fff` and walks a linked structure
until a type match. On this drive the ROM table is empty (test image
zeros), so the walk faults — confirming it needs a real measured
operand, not a synthetic zero ROM.

## Verdict

**DEFER.** The shortcut replaces the 248-instruction long body with
`counter++` + `0x18bd4` (itself multi-call) + exit at `0x230b8`. That
is still a multi-block subtree, not a compact prefix leaf.

Does not affect the warm PUNCH pin (`0x225cc` is not reached when both
contact-query results are zero).

## Proof

- Guest trace: shortcut taken, `call 0x18bd4`, nested `call 0x1ab34`
- No C recovery in this commit
- No snapshot/trace/ROM data is committed
