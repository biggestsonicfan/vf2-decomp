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
