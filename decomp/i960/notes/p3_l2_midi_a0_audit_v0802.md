# P3: L2 worker, countdown re-verify, L3 MIDI contract, A0 — audit (v0802)

## Scope

P3 covers four items:
1. L2 worker (0x21ba): the L2 driver and its worker-forward
2. Countdown re-verify: the countdown/emit half (dead per v0797)
3. L3 MIDI contract: the MIDI input/output path
4. A0: the A0 class dispatch (refuses at entry per v0795)

## Audit results

### 1. L2 worker (0x21ba)

| aspect | C state | silicon (per v0797) | C correct? |
|---|---|---|---|
| L2 driver entry | `vf2_sound_board_maintain_streams` (line 655) is a no-op | L2 driver at 0x21ba enters via vector, worker-forwards to 0x2242 with preempted a0 | YES — C correctly no-ops (dead countdown/emit per v0797) |
| Preempted a0 | not modeled | L2 driver enters 0x2242 with preempted a0 | YES — boundary (C doesn't model preempted register state) |
| Worker self-disarm | not modeled | worker self-disarms at 0x220e | YES — dead in practice |

The C's no-op `vf2_sound_board_maintain_streams` reports the
write cursor for documentation of stillness. This is correct
per v0797's "dead countdown/emit" finding.

### 2. Countdown re-verify

| aspect | C state | silicon (per v0797) | C correct? |
|---|---|---|---|
| Countdown/emit half | not modeled | dead: +0x0c is boot-cleared, no store of nonzero | YES — correctly not modeled |
| Prescaler abort | not modeled | prescaler aborts to main loop at slot 0 | YES — correctly not modeled |

Per v0797: "the countdown/emit half stays dead: +0x0c is
boot-cleared and no store of nonzero to any stream desc +0x0c
exists (d16-dst scan: only primer READS at 0x8d2/0x8e4, a
0x3000-table RMW at 0x196c/0x19a8, and the F-path +0x0e
store; indexed-dst 0x0c hits are bit-ops/reads/coincidences).
Worker self-disarms at 0x220e."

The C correctly does not model the countdown/emit half
because it's dead. No C-side fixes needed.

### 3. L3 MIDI contract

| aspect | C state | silicon | C correct? |
|---|---|---|---|
| MIDI FIFO | `vf2_scsp_midi_receive` (scsp.c:397), `vf2_scsp_midi_read` (scsp.c:412) | SCSP has MIDI input FIFO at 0x0404/0x0405 | YES — SCSP model has MIDI FIFO |
| MIDI path in sound board | not used by `sound_board.c` | main CPU writes MIDI data | YES — MIDI is via SCSP bus, not sound board |
| MIDI pending bit | `update_midi_pending` (scsp.c:241) | SCSP has MIDI pending bit | YES — SCSP model has pending bit |

The SCSP model has the MIDI FIFO and pending bit. The
sound board does not use MIDI directly. The MIDI path is
through the SCSP bus model (0x0404/0x0405 reads). No
C-side fixes needed.

### 4. A0 dispatch

| aspect | C state | silicon (per v0795) | C correct? |
|---|---|---|---|
| A0 entry | `sound_board_dispatch_a0` (line 816) refuses UNSUPPORTED | 0x1e74: body splits on cmp.l d2,d0 with d2 never loaded | YES — unmodelable, refuse at entry is correct |

The C's A0 dispatch refuses at entry per v0795. The comment
documents the formulas kept in the master note. No C-side
fixes needed.

## Conclusion

P3 audit: **no C-side fixes needed**. All four items are
correctly handled in the current C:
1. L2 worker: no-op (correct per dead countdown/emit)
2. Countdown re-verify: not modeled (correct, dead)
3. L3 MIDI contract: SCSP model has MIDI FIFO (correct)
4. A0: refuses at entry (correct, unmodelable)

The working-tree state is unchanged from the v0801 P0c
closure. No `src/` modifications.

## Files

- `decomp/i960/notes/p3_l2_midi_a0_audit_v0802.md` — this note
