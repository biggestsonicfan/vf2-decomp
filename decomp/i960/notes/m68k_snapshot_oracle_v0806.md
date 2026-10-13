# v0806 - Slice B: 68k oracle save/resume (snapshot + replay proof)

Status: implemented + ROM-proven. Closes the last Slice B gap.

## What already existed (not rebuilt)

- `vf2_m68k_run` (stop_pc / stop_on_return / max_steps / steps_done).
- Real-ROM differentials: triangle L1 (5 legs) + e0 tail + uploader,
  loopb stepper pins. These are the reference-vs-recovered pattern.

## What was missing and is now added

`vf2_m68k_snapshot_{init,capture,restore,equal,destroy}` in
`include/vf2/m68k_step.h` / `src/m68k/stepper.c`:

- Captures regs + full RAM image + SCSP model state + bus event log.
- SCSP copies shallow: sample ROM/RAM attachments are run inputs, not
  state (documented in the header).
- Restore requires equal RAM size, else `VF2_ERROR_INVALID_ARGUMENT`
  (fixture mismatch fails closed).
- Equality is field-wise throughout (regs, SCSP slots/midi, events):
  struct `memcmp` is avoided because `regs_reset` and `log_event`
  leave padding indeterminate.
- The C side needs no helper: `vf2_sound_board` state is the board
  struct and copies with assignment.

## Proof (ctest `vf2_m68k_snapshot_differential`, ROM-backed)

Fixture is the triangle L1 s1-clean leg (entry `0x5fe`, 200 steps
pinned, `a7` balanced, zero bus events):

1. pre-run capture vs post-run capture compare **different** (run
   changed state; comparator sees change).
2. restore pre + replay gives identical status, identical step count
   (200 == 200), and a snapshot **equal** to the first post-run
   (determinism + restore fidelity).
3. one flipped RAM byte compares **different** (negative control: the
   comparator can fail).
4. restore into a 16-byte bus refuses `VF2_ERROR_INVALID_ARGUMENT**;
   NULL args refuse; `equal(NULL, x) == 0`.

Both comparator polarities are asserted live, so a stuck comparator
fails the test either way — no planted-defect check needed.

## Not done (deliberately)

- Boot `0x28e` init differential: `initialize()` still fails on the
  real ROM (uploader never ran against real bytes; triangle T3 note).
  Unchanged by this slice.
- FM boundary measurement (v0805 consequence 2): next slice.
- File persistence for snapshots: not needed until a multi-session
  probe workflow asks for it; in-memory covers the differential use.
