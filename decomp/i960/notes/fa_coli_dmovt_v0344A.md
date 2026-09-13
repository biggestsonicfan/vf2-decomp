# v0344-A: executor `dmovt` + `mulo` overflow latch; `0x502a4` traced

## Verdict

Slice A landed (executor only, no game semantics changed):
`dmovt` reg-reg double copy + sticky overflow latch on `mulo`.
The v0341 halt is gone; the reference walks the digit loop and
exits. Slice B (native `0x502a4` helper) is fully specified below
but not implemented: the `bx`-out continuation (`0x7fc0`,
`0x9444` subtrees) is unrecovered, so both coli gates correctly
stay fail-closed.

## `dmovt` (measured form only)

- Site: `0x508d4`, word `0x64181203` = `dmovt r3, r3` (src bits
  0-4, dst bits 19-23; derived from `decode_register`, not guessed).
- Implemented as a 2-register copy mirroring `movl`, no
  compare-state effects (mirrors `mov/movt/movl/movq`), reg-reg
  only, everything else `UNSUPPORTED`.
- Unit (`tests/i960/test_executor.c`, codes 54-59): exact ROM word
  (IP+4, neighbours intact, `OVERFLOW` sentinel preserved) plus a
  derived `dmovt r4, r8` (`0x64401204`) proving the pair copy and
  flag neutrality the following `bo` depends on.

## `mulo` sticky overflow (assumption grade: architecture-inferred,
pins-validated — read this before reusing)

- The digit loop (`ldob (g1)` → `dmovt` → `bo` → `mulo/subo/addo`
  ×2 → `b`) has exactly one conditional (`bo`, overflow latch).
  In the old oracle nothing set the latch → infinite spin (365+
  identical iterations observed); hardware must exit via `mulo`
  overflow (`g0` grows ×10/iter).
- Elimination evidence for `mulo`-V over `dmovt`-sets-flags: any
  `r3`-test in `dmovt` would exit by iteration 12 (first `0x00`
  byte) or iteration 1 (first non-digit); the loop demonstrably
  runs past 350+ zero/non-digit bytes. Only accumulating-overflow
  fits. The `dmovt`-before-`bo` placement stays unexplained but is
  behaviorally neutral here (self-move); the implementation
  restricts to reg-reg and claims nothing more.
- Rule: unsigned 64-bit product `> UINT32_MAX` sticky-sets
  `OVERFLOW`, otherwise the latch is untouched (narrowest blast
  radius: non-overflowing mulos, including every pack `mulo` in
  recovered paths, are bit-identical). `muli`/`subo`/`addo`
  deliberately untouched. Sticky (not updating) is forced by
  termination: updating-then-clearing in `subo`/`addo` could never
  deliver `mulo`'s V to the next iteration's `bo`.
- Validation: full `ctest -C Debug` 56/56, PUNCH 320/320 MATCH,
  input-17 64/64 MATCH — green everywhere, so no measured path
  overflows a `mulo` before a latch-branch. Re-scope (revert this
  hunk) if any pin ever moves.

## Measured continuation (reference, `out/v0344/cont.jsonl`)

- `bo` falls through on iteration 1 (stale scanbit `NONE` — matches
  the flag-neutral `dmovt`), loop runs 9 iterations over
  `0x22951` `"d hit combo"`, exits when `510236973*10` overflows:
  `bo` taken → `0x508f0 cmpobne` → ret. Exit `g1 = 0x2295a`,
  `g0` = Horner garbage (exact word belongs to the slice-B unit,
  computed by the now-terminating oracle).
- Site A span: trace 57-197 = **141 steps**, `0x22948 balx` →
  frame setup → `call 0x502c0` → compares → `call 0x508c4` →
  digit loop → `0x508f0` → tail compares/stores → `ret` →
  epilogue → `bx (g2)` → **`0x22960`** (computed return; the
  `0x22950 sqrtr` + 5 following bytes are skipped, likely dead).
- Site B fires in the SAME drive (trace 510+): `0x22e04 balx`
  (with `g9 = 0x0100085e`, `g1 = 0x00503200`, a ROM string) →
  `0x502a4` entered twice total. Same digit-loop shape.
- Past `0x22960`: `call 0x7fc0`, `cmpobne`, `balx 0x9444` — separate
  subtrees, explicitly out of scope.

## Slice-B recipe (queued, all evidence in hand)

1. Standalone C helper in `hybrid.c` (balx entry → `bx`-out exit,
   ending `ip = g2`), privately modeling: frame setup, `0x502c0`
   compares/stores, digit loop with host-side overflow check
   (`(uint64_t)g0*10 > UINT32_MAX` → break, mirroring the oracle
   rule), epilogue `g2` computation. Control-flow + memory
   footprint: span branches/stores enumerated from `cont.jsonl`
   trace 57-197 (site A) and 510+ (site B).
2. Two units in `test_coli_225cc_long` style (fresh function; reset
   every field): site-A drive (`f1+0x1a4 = 0x4000`, board
   `0x8800`) and site-B state, exact counts/stores from the trace.
3. Do NOT wire the coli gates until the `0x22960+` continuation
   (`0x7fc0`, `0x9444`) is recovered — the caller cannot complete
   past `bx`-out today, and an admitted-but-stranded path would
   violate fail-closed for zero behavior gain.

## Proof (slice A)

- `vf2_tests`: all pass (new `dmovt` codes 54-59).
- `ctest -C Debug`: **56/56**. PUNCH **320/320 MATCH**.
  input-17 **64/64 MATCH**.
- Halt probe `out/v0344/halt.jsonl`: 81 steps → `0x508d4`
  (pre-change baseline, kept).
- Continuation `out/v0344/cont.jsonl`: 3000 steps, loop exit at
  trace 154, ends at frame wait (scratch only, not committed).

## Still fail-closed

- Both `balx 0x502a4` sites (continuation unrecovered).
- `0x22950 sqrtr` region (never executed on measured paths).
- Non-register `dmovt` forms; `muli`/`subo`/`addo` flag effects.
