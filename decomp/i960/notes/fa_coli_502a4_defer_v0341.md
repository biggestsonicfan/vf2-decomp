# v0341: coli `0x502a4` balx sites — measured deferral

## Verdict

Keep fail-closed. The two `balx 0x502a4` sites are correctly
rejected: the reference itself halts inside the subroutine on an
unimplemented executor instruction (`dmovt` at `0x508d4`).
Recovering this path is an executor-extension campaign
(`dmovt` semantics + the `0x502a4`/`0x502c0`/`0x508c4` subtree),
not a coli residual. No game semantics are invented here.

## Measured facts (reference `vf2probe`, `out/v0341/`)

Two ROM call sites target `0x502a4` (maincpu ROM code, not a
special address space):

- Site A `0x22948` (cascade region): reached with bit-13-clear
  flags and board `0x508000` bit 9 CLEAR. Gated on board bit 9
  only — no fighter-flag gate on this route. Bit 9 set takes
  `bbs 9 → 0x2298c` (warm join, native).
- Site B `0x22e04` (`0x22dec` region): needs `g8+0x1a4` bit 14
  plus board bit 9 clear (v0338). Not reached on the site-A
  drive (the cascade balx fires first).

Trace (`balx2.jsonl`, `f1+0x1a4 = 0x4000`, board `0x8800`):

```text
entry → pack → 0x226b4 bbc-4 taken → 0x226f4 → 0x226f8 bbc-13
taken → 0x22914 cascade (mov/counter/board) → bbs-9 nt →
... → 0x22948 balx 0x502a4 → frame setup → call 0x502c0 →
... → call 0x508c4 → mov/addo/mov/ldob → 0x508d4 dmovt r3, r3
```

Halt after 81 steps: `unsupported instruction` at `0x508d4`
(`dmovt`, decimal move). Next ROM instruction is `bo 0x508f0`
(decimal/overflow branch). The `function` command sees only one
block for `0x502a4` (`indirect=yes`); the subtree needs
trace-guided attribution like the `0x23524` callees (v0278).

## Unblock recipe (queued, not this slice)

1. Implement `dmovt` in `src/i960/executor.c` from the Intel i960
   manual (operands, condition codes, faults), with a
   ROM-independent unit test in `tests/i960/test_executor.c`.
   Do NOT guess semantics from this trace alone.
2. Re-probe both drives past `0x508d4`; attribute the
   `0x502a4`/`0x502c0`/`0x508c4` subtree glue-by-glue.
3. Recover the smallest native prefix (likely the frame setup +
   first call), keep siblings unsupported, unit + pins.
4. Only then wire the two coli call sites.

## Proof of correct fail-closed

- Native returns `VF2_ERROR_UNSUPPORTED` at both sites
  (cascade board check, `0x22dd4` board check).
- PUNCH / input-17 / ctest unaffected (warm has bit 9 set).
