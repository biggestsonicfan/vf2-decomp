# The attract freeze is the TGP self-test phase (selector 17) waiting on the TGP

Date: 2026-10-14. Evidence-only analysis (no recovered C changed), reproducible
from the ROM. This is the root of the attract -> 3D blocker; it ties together
attract_geometry_gate_chain.md and attract_interrupt_injection_vs_mame.md.

## The attract frame dispatch and the frozen phase

The per-frame attract dispatch is at `0x0000a6c0`:

```
a6c0  ld   0x00508000, r15
a6c8  bbs  5, r15, 0xa6f4        ; if bit 5 of 0x508000 set, skip dispatch
a6cc  ldob 0x0050002a, r3        ; r3 = phase index
a6e8  ld   0xa6f8[r3*4], r5      ; jump table at 0xa6f8
a6f0  callx (r5)                 ; run the phase worker
a6f4  ret
```

Measured over a natural boot: `0x508000` bit 5 stays clear (dispatch runs every
frame), and the phase byte `0x0050002a` advances `0x00 -> 0x01 -> ... -> 0x11`
by frame ~20 and then **freezes at 0x11 (17)**. The jump table at `0xa6f8`:

```
phase 0x11 -> worker 0x00010b5c
```

## The phase-17 worker and its gate (0x5000a6)

`0x00010b5c`:

```
10b5c  ldob   0x005000a6, r3
10b64  cmpobe 0, r3, 0x10b88     ; if 0x5000a6 == 0 -> call 0x00055008 (advance)
...                              ; else            -> call 0x00058fe0 (wait)
```

`0x005000a6` is `0xff` (nonzero) throughout attract, so every frame the worker
takes the wait branch and calls `0x00058fe0`. Crucially, a read-poll trace shows
`0x5000a6` is **read 1x/frame and never written** during attract: nothing clears
it. The phase advances only when it becomes 0, and the thing that would set it
to 0 is not reached.

`0x00058fe0` is the geometry-submission worker: it reads the geo-gate state
(`0x500700`/`0x500704`), an index `0x5000a4`, and a descriptor table at
`0x0005feac[idx*8]`, and builds command lists into the geo buffers. In other
words selector 17 submits a geometry test to the TGP and spins until the TGP
signals completion (which would clear `0x5000a6`).

## Why this is the blocker (and why it is TGP-first)

Selector 17 is the TGP self-test / init phase (`selector17_index8_tgp_test.md`,
`selector17_index5_coin_assignment.md`). In MAME the TGP receives its program at
boot (frame 11, ~8200 writes to 0x804000) and the test passes, so the attract
leaves selector 17 and proceeds (loading 727, 3D 967). In our native the TGP has
no program (the boot's 0x00980000 bit-31 upload pulse fires before the geo-port
writes, so the program is never captured) and no coprocessor response, so the
self-test never completes, `0x5000a6` never clears, and the attract is stuck in
selector 17 forever.

This resolves the chicken-and-egg recorded in `docs/NATIVE_WINDOWS_PLAN.md`
Phase 6 Status: the game cannot reach a 3D scene (to generate geometry) until it
passes the TGP self-test, which needs a working TGP. **The TGP must come first**
-- i.e. G2 / Option 2 (lift the MB8623x microcode, upload its program, model the
copro response), not a native pipeline built from a geometry stream the stuck
attract never emits.

## Concrete next targets

1. Find what clears `0x005000a6` (the TGP-test "pass" writer) and what it reads
   to decide the test passed -- that names the exact TGP response the native
   must produce. Search for a store to `0x5000a6` reachable from `0x00055008`
   or the copro-result path.
2. Fix the boot TGP program capture: the ROM signals upload via `0x00980000`
   bit 31; determine the real bracketing so the ~8192 program words land in the
   program buffer (`geometry_program_count` should become nonzero), a
   prerequisite MAME satisfies at boot.
3. With the program captured, a lifted/interpreted MB8623x can run the test
   geometry and produce the response that clears `0x5000a6`, letting the attract
   advance -- the first time real geometry would be emitted natively.
