# Attract -> 3D: vectors 14/15 are necessary but not sufficient (vs MAME)

Date: 2026-10-14. Evidence-only analysis (no recovered C changed). Driven by a
MAME vf2 reference session (live, user-supplied): at boot the TGP receives its
program (frame 11: ~8200 writes to 0x804000, 3803 to FIFO 0x884000, 154 to
0x800000); during the attract wait (frames ~264-726) the game takes, besides
VsyncScr (vector 12), ~two more interrupts every ~2.4 frames; MAME timeline:
sound commands at 725-726, loading at 727, 3D at ~967.

## Interrupt table (confirmed, matches MAME)

From the cold snapshot and a natural reset boot, PRCB interrupt table is at
`0x005ff000`, handlers:

- vector 12 -> `0x00000bc0` (VsyncScr / frame)
- vector 14 -> `0x00000d50` (timer; request bit 5; sets `0x0050008c`)
- vector 15 -> `0x00000e10` (sound; request bit 10; drains the sound command
  ring `0x00504020` to the serial port `0x01c80000`)

## What our native does today

The native frame scheduler injects only vector 12. Vectors 14 and 15 are never
raised, so the timer tick and the sound-queue drain never happen.

## Experiment: inject vectors 14 + 15 at the MAME cadence

Raising request bits 5 / 10 and entering vectors 14 / 15 as a pair once every
2.4 frames (MAME's rate), alongside the normal vector-12 injection, both (a)
from the recovered sixth-dispatch snapshot and (b) from a natural interpreter
reset boot:

- vector 14 runs correctly: `0x0050008c` goes `0x01000000 -> 0x01000001` (bit 0
  set), matching MAME's "sets 0x50008c".
- but over 1100+ frames the attract does **not** advance: `0x00500704` bit 2/26
  stay clear, byte `0x005000a6` stays `0xff`, `frame_geometry_gate` keeps
  skipping (see attract_geometry_gate_chain.md), and object submission
  (`0x7c60`) / the display-list builder (`0x61ba0`) are never reached.

So vectors 14/15 are **necessary** (MAME fires them) but **not sufficient** to
drive the attract -> 3D transition in the current model.

## Leading hypothesis for the missing piece

The MAME transition order is: sound commands (725-726) -> loading (727) -> 3D
(967). Our vector-15 handler drains the sound ring to the serial port
`0x01c80000`, but nothing consumes it: there is no live 68000/SCSP sound board
reading that port and acknowledging (the shipped decision keeps the sound board
as a validation oracle, not a live responder). If the attract "loading" at 727
is gated on the sound board having consumed/acked the command stream, the native
would stall exactly as observed. The TGP program is also never uploaded in our
boot (the `0x00980000` bit-31 pulse the ROM emits fires at reset, before the
geo-port writes, so it does not bracket them), which 3D would additionally need.

## What is needed to pin the trigger

1. A MAME **RAM-state** capture around frame 727 (the user's
   `vf2_attract_frame_geometry_f1000.txt` is a geometry-write trace, not a RAM
   dump): the values of `0x00500704`, `0x005000a6`, `0x0050008c`, and whatever
   sound-ready / loading flag the main loop polls to leave the attract wait.
2. Or the sound-board command-consumption / ack path, so the native reproduces
   the handshake the loading phase waits on.

Reproduce: the interpreter frame loop from `out/cold.vf2snap` (or a reset boot)
with `vf2_hybrid_frame_wait_observe` for vector 12 plus manual
`vf2_model2a_raise_interrupt` + `vf2_i960_cpu_enter_interrupt` for vectors 14/15.

## Follow-up: the attract phase machine freezes (not interrupt-driven)

Tracing what the attract loop actually polls (memory-read observer, 120 frames
from the cold snapshot): it never reads the timer flag `0x0050008c` (0 reads) or
the sound ring `0x00504020` (0 reads/writes), so the vector-14 timer tick is
irrelevant to advancement here. What it polls heavily is the sel3 phase state:
`0x00500028` (597 reads, 119 writes), `0x00500704` (715 reads, the geo gate) and
`0x005000a6` (119 reads). Advancement is **phase-state driven** (per v0366), not
interrupt driven.

And the phase state is **frozen**. Sampling `0x00500024` / `0x00500028` /
`0x00500834` / `0x00550000` over 1000 frames, in **both** the cold snapshot and
a natural reset boot: the attract phase initialises by ~frame 30 to

```
0x500024 = 0x0000027f   (a countdown; v0366 phase 3 says it should fall ~1/frame)
0x500028 = 0x11110000
0x500834 = 0x00515b00   (task pointer, matches v0366)
0x550000 = 0x00000000
```

and then never changes through frame 1000. The countdown `0x500024` does not
decrement, so the per-frame phase worker that would advance the attract is not
running (or is gated before it decrements). This -- not the interrupt cadence --
is the concrete freeze point.

Next step to pin it: (a) in MAME, does `0x500024` fall from `0x27f` (or 0x100)
during attract frames 30-967, and what does `0x500028` become -- i.e. is the
phase worker running there? (b) in the native, find the routine that decrements
`0x500024` and why it is not reached each frame (same method as
`frame_geometry_gate` in attract_geometry_gate_chain.md). The freeze is upstream
of both the geometry commit gate and the interrupt handling.
