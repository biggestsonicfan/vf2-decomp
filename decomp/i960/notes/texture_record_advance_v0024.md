# v0.0.24 texture record advance

## Observed path

The active texture-record loop reaches `0x0004bf60` four times. On every
visit, `r6` is one and the signed halfword at record offset `0x02` is positive.
The block decrements the remaining count, advances the two stream pointers by
four bytes, writes all three values back to the record, and branches to
`0x0004bd24`.

The exact observed path is 12 instructions per visit, for 48 recovered
instructions across four visits. It writes 10 bytes per visit and performs no
procedure calls or returns. Unsupported counter or loop states are rejected.

## i960 comparison semantics

`cmpdeco 1, r6, r6` compares one with the original value and then decrements
the destination. Because the observed original value is one, it leaves the
architectural comparison state equal. The following `cmpibg` and `cmpibe` are
COBR-format direct comparisons: they select branches without replacing that
comparison state. The recovery therefore preserves equal through the exit.

## Validation

The public bridge test covers the full CPU and memory post-state, including
`g0`, `r6`, `r8`, `r9`, `r10`, the preserved equal comparison from `cmpdeco`,
the halfword count, and both 32-bit pointers.

The exact VF2 2.1 ROM-backed `native-second-dispatch` validator executed 12
reference i960 instructions for each of the four visits and reached full CPU
and Model 2 memory `MATCH`. The strict totals are now 1,269,003 recovered and
1,819 interpreted instructions, with 172 recovered blocks and memory
checkpoints. Calls and returns remain 266 / 300.
