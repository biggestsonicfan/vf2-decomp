# v0344-B: native `0x502a4` digit-parse helper + direct unit

## Verdict

Slice B core landed: `coli_502a4_body` + `vf2_hybrid_coli_502a4_execute`
recover the full balx-to-bx subtree natively for BOTH sites, proven by
a direct ROM-independent unit (exact counts, exit registers, stores,
bx targets, negative control). The coli wrapper gates are UNCHANGED
(both balx sites still fail closed there); wiring is queued behind
the `0x22960+` continuation campaign (see below).

## What the code does

Entry at the balx target (`0x502a4`) with caller registers
(`r14` = balx link, `r15`, `g1` = output buffer). Exit at the
computed `bx` (`ip_out` = `g2`), no frame push/pop (balx/bx keep the
caller frame; the execute wrapper advances counts directly instead
of `hybrid_complete_procedure`). Internal calls/returns
(`0x502c0`, `0x508c4`) are balanced (+2/+2).

Stages: prologue frame dance → `0x502c0` head (r10 = link walks the
inline length-prefixed data, r11/r5 = buffer) → digit loop
(`0x508c4`, Horner x10 with oracle-latch mirroring: `bo` reads the
sticky overflow set by the PREVIOUS `mulo`) → `0x50328` or-chain →
post-loop byte classification (all four `cmpobe` fall through) →
NUL-terminated copy loop back into the buffer → align-up
(`subo` operand order: dst = src2 - src1, i.e. `4 - (r10 & 3)`) →
epilogue `bx (g2)`.

## Key findings (from measurement, not guessing)

- Entry `g1` is the OUTPUT buffer (`0x00503200`), not the string:
  forced by the copy stores (`0x503200+` written while `g1` was
  restored from `r5` = entry `g1`), forensically confirmed by both
  callers' `lda 0x00503200` prefix. The digit loop instead parses
  via `r10` (derived from the balx link) after `0x50308` resets
  `g1` to the string.
- The prefix `st r3,(sp)` + `0x50368 ld (r9)` stack slot is
  write-then-read-DISCARDED: the loaded `g0` is clobbered at
  `0x502fc` before any read/store/branch, and `bbc-31` reads the
  Horner `g0` (verified order). `sp` is untracked (helpers have no
  cpu access); the slot is accounting-only with counts retained.
  Final `g0` is DETERMINED (`r11` = buffer), not garbage.
- `g0/g2` entries, `g7/g8`: proven irrelevant (overwritten before
  any read; no fighter access in span) and not taken as inputs.
- The align math rounds the copy pointer UP (`0x2295d -> 0x22960`,
  `0x22e1e -> 0x22e20`); the `bx` target is fully computed, and the
  execute wrapper asserts nothing about it beyond the model (unit
  checks both constants).

## Provenance (all measured in `out/v0344/cont.jsonl`)

- Site A (trace 57-197): link `0x22950`, inline `25 64 20 68 69
  74 20 63 6f 6d 62 6f 00` ("d hit combo"), 9 loop iters, 2-byte
  copy (`6f 00`), bx-out `0x22960`, helper span 140 steps.
- Site B (trace 510-680): link `0x22e0c`, inline `25` + "d down
  hit" + `20 63 6f 6d 62 6f 00` at `0x22e17`, 10 iters, 7-byte
  copy (" combo\\0"), bx-out `0x22e20`, helper span 170 steps.
  Same branches as site A (only `0x502c0`/`0x508c4` calls, all
  classification `cmpobe` fall through): one data-driven helper
  covers both.
- `subo` operand trap re-confirmed: `subo r15,4,r15` = `4 - r15`
  (matches `0x22960`; the naive order gives `0x2295e`).

## Unit (`test_coli_502a4`)

Both sites driven through `vf2_hybrid_coli_502a4_execute` with
synthetic ROM inline bytes: exact step deltas (140/170), exact
exit regs (`g0` = buffer, `g1` = buffer+2/7, `g2` = bx target),
exact `ip`, +2/+2 calls/rets, exact store words, plus a negative
control (classification byte forced to `0x64` takes the `0x503b4`
call edge → `UNSUPPORTED`).

## Wired (v0344-C): site-A wrapper path

Wrapper gate admits scan-1 + bit-13-clear + bit-3-clear + no15/16
+ board-clear into `long_body` (extra board read fires solely on
the newly-admitted path, `v0340` precedent). The shared prefix
needed no new work (bit-3/bit-13-clear edges already native from
`v0314`/`v0343`). At the cascade `bbs-9`-nt edge the site runs
the prefix + helper and fail-closes past bx-out (`0x22960`
target-checked) with the helper's stores applied. Wrapper unit
(L1 setup with bit flips + planted inline bytes): `UNSUPPORTED`
status, `+0x6d8` counter = 1, copy bytes `6f 00` at `0x503200`.
Site B keeps its own gate (prefix crosses the continuation).

## Queued (not this slice)

- The `0x22960+`/`0x22e20+` continuation (`call 0x7fc0`,
  `cmpobne`, `balx 0x9444`, …) is the actual next frontier.
- Site-B wrapper wiring needs its caller prefix (through the
  continuation above); its helper path is already proven by the
  direct unit.
- Sanitizer gate: `build-san` (clang-cl) cannot find CRT headers
  pre-existing environment limitation, unrelated to this change.
  Strict MSVC build (warnings-as-errors) + full ctest + both pins
  green; the helper uses only established checked accessors and
  bounded loops.
