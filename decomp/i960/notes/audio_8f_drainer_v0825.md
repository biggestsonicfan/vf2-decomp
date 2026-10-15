# 8f: sound-queue UART drainer recovered (v0825)

Status: `vf2_hybrid_sound_43800_execute` in hybrid.c + decl in
player.h, 8-leg ROM differential + unit legs green
(`vf2_sound_43800_drainer`, ~1 s). Unwired (standalone, v0741
posture). Full suite in the commit validation.

## What it is

The i960 queue manager lives at 0x43740-0x43b3c: init/fill
(0x43740), UART init + IRQ ack (0x437d0: pokes 0xe80004 with 33,
clears line-10 bit 0x400 at 0xe80000), the drainer (0x43800),
the filer (0x43888, already native since v0746) with its queue
write (0x438fc: 16-entry ring at 0x504020 stride 4, count
0x504001, wcursor 0x504003, rcur 0x504002, staged 0x504010,
byte-idx 0x504014), the dedup helper (0x439ac, v0745), and the
g13/fighter-state filer drivers (0x43950+, 0x43a9c+: g13 bits
30/27/28 and fighter +0x1b1/+0x624 gate table lookups into
0x43888). The filer has 80 static callers; the drainer has ZERO
(direct or executed -- 0 hits in 100k gameplay steps) and runs
only via the boot fall-through: queued entries pile up and
never forward in every measured context.

## Three corrections this slice made

1. **UART address: 0x01c80000, not 0x009c0000.** The drainer
   stages `lda 0x01c80000` (Model 2 8251 on the secondary bus;
   the 0x9c address is the 2B-CRX part). The whole 8c hunt
   watched the wrong window -- every "silent" conclusion about
   WRITES stands (no writes to either address), but the
   table-read watch covered the wrong tables for the writer.
   The static 0x009c0000 table refs remain real but unreached.
2. **0x01c80000 is modeled** (inside IO-control RAM: accepted
   flat, no device effect). The store/pokes/ret all execute for
   real -- full recovery, no refusal. The byte reaches no 68k:
   bridge wiring (byte -> L3-equivalent ring file) is still open.
3. **Branch rule: load iff idx < 0 (signed).** Three wrong
   theories died on the differential: `== 0` (mid fails),
   `<= 2` (idle5 fails), `== 0xff` (high fails: 0x80 loads).
   The printed `cmpible 0` reproduces none of them; the
   decoder's condition on this form (le vs ge) is suspect,
   behavior pinned over idx {0,1,2,3,5,0x80,0xff} x entries
   {0,1,2}. With subi/stib wrap (0 -> 0xff) every entry yields
   [2],[1],[0] and 0xff is the "entry exhausted" sentinel.

## Conventions (deviations from v074x, all differential-driven)

- No pushed frame on either side (439ac pushes fake frames):
  the recovery never steps the ret, and the reference harness
  parks on ret arrival, so registers and frame liveness compare
  directly. Returning through a frame would wipe r3/r4/r5 via
  restore (measured: C 0xdead vs S ROM-values) and hide the
  proof. Documented in the decl; unwired so no dispatcher
  depends on a pop.
- Body counts exclude the unstepped ret (hand-step deltas match
  exactly: 4/11/23). The stepper does not count ret; the
  439ac-style include-ret passed only self-consistently.
- r6 is the live-caller contract (uart-init and filer-common
  both establish 0xe80004); the C hardcodes the poke address
  and never reads r6, and the test stages + verifies it.

## 8/10 status

Bridge pieces now native: filer (v0746), drainer (here),
dispatch/drain/render (S2-S4). Missing: L3-file (0x526,
mapped, unrecovered) and any live trigger. The synthetic chain
(queue -> bytes -> ring -> voices -> WAV) is now composable
entirely from stepper-proven C -- that composition is the next
slice.
",
