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

## The TGP microcode is not in our ROM set

Capturing the ~8200 words the boot writes to the geo port `0x00804000`: they are
all to the single port address and are **mostly zero** (first words
`00000000 00000800 00000000...`), i.e. a geo buffer/command fill, **not** dense
MB86233 instruction words. And `VF2_REGION_COPRO_DATA` is zero-filled (no files
in the manifest). So the MB86234/TGP microcode is **not present** in our romset,
and it is not uploaded as code by the i960 either. MAME's "the TGP receives its
program at boot" is satisfied by a TGP program ROM MAME has that our manifest
lacks (the Model 2A TGPx4 program), or by the chip's internal ROM.

Consequence: **both** Phase 6 options are blocked on the same missing input --
Option 2 (lift the microcode) has no microcode, and Option 1 (native pipeline)
has no geometry stream because the attract never leaves the TGP self-test. The
two ways forward are:

- **(a) obtain the TGP program ROM** (from a complete MAME vf2 romset -- the
  likely-missing copro/TGP program file) and add it to the manifest, then do
  Option 2 (decode/interpret/lift the MB86233 and model the copro response); or
- **(b) model only the TGP self-test response as HLE-TEMP scaffolding**: clear
  `0x005000a6` when selector 17's test geometry has been submitted, so the
  attract advances to 3D and emits the real geometry stream (matching the
  user's `vf2_attract_frame_geometry_f1000.txt`), which then feeds Option 1.
  This needs the exact clear condition: `0x5000a6` has no static xref (it is
  written through a computed base+offset), so finding it needs a dynamic trace
  of the store, or the MAME answer for what writes `0x5000a6 = 0` and when.

## Where the TGP microcode actually lives (not in the game romset)

Cross-checking the romset against the manifest: every `epr-/mpr-/opr-` file is
mapped, and the only unmapped entries are the `vf2a` / `vf2b` / `vf2o`
subdirectories, which hold *other game revisions* of the i960 main program
(`epr-17562..17569`, 128 KiB each) -- not TGP data. The `opr-1474x` files are the
TGP math tables (sin/cos, atan, 1/x), and `mpr-1631x` are the video-board
lookup. There is **no copro/TGP program ROM in the VF2 romset**, and
`VF2_REGION_COPRO_DATA` has no source file (it is zero-filled).

So the MB86234 (TGPx4) microcode is the **chip's internal program**, the same
across Model 2 titles, which MAME carries as a device ROM (not part of a game's
romset). Obtaining it is a chip-microcode / clean-room question for the
maintainer, distinct from the game ROMs the user supplies. Without it, Option 2
cannot decode/lift a program, and the only in-tree route is (b): a faithful
HLE-TEMP model of the selector-17 self-test response -- which, as measured, must
reproduce the TGP's actual result (a flat `0x5000a6=0` poke advances one sub-step
then the test re-arms), not just a flag.
