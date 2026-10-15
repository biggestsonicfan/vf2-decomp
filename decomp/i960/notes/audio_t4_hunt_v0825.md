# T4: live-writer hunt (trigger plan)

Status: boot chain fully evidenced (both writers named with
call sites); gameplay contexts all clean negatives. Hunt
driver (`test_session_queue_hunt`, `step_oracle_until` + work
-RAM accessor) is the lasting tool. Full suite in the commit
validation.

## Hits (boot.vf2snap, no input, 30-frame budget)

- Flusher 0x43abc: frame 17, 5,692,876 steps, g13 = 0x515d80,
  ret = 0x10dcc -> call site 0x10dc8: `callx (r4)` with
  r4 = task-record [g13+0x0c]. The flusher runs AS A TASK BODY
  through the scheduler dispatch (the documented 0x10dcc
  380/9/10 point).
- Filer 0x43888: frame 18, 5,714,298 steps, g0 = 0x00ae101f,
  g13 = 0x516480, ret = 0x58eb0 -> site 0x58eac:
  `lda 0x00ae101f, g0; call 0x43888` (T1 cluster-16,
  sound-test helper family). Files entry 3 (count 2 -> 3 =
  the park state).
- Both arrivals see count 2 / wcur 2 / ring0 = ae101f:
  entries 1-2 predate frame 17 (entry-2 9e0a7f filer still
  open: 0x009e-family immediate sites or an earlier
  flusher-table pass).

## Negatives (all healthy driver runs, EXIT 0)

- sixth-fresh + PUNCH pattern x10 frames: no arrival.
- sixth-fresh + KICK held x30: no arrival.
- sixth-fresh + START edge (128/0) x30: match-start BGM does
  NOT file here -- the 8c prime suspect is cleared for this
  snapshot/input pairing.
- player-29414-sixth single frame: the 0x295e0 ad231f site
  not reached (guarded path, needs its g1/entry state).

## Tool notes

- `step_oracle_until`: IP-breaking oracle frames (arrival
  checked pre-step; per-frame input cycle; 50M fail-closed
  cap). Miss returns OK + hit 0.
- Return address comes from the pushed frame's r2 slot, NOT
  the working set (the executor zeroes callee regs at entry
  -- first cut read zeros, corrected against procedure_call).
- a0-class note for future hunts: arrivals name the DOOR,
  not the outcome; check the handler.

## Left open

- Entry-2 (9e0a7f) filer: earlier-boot hunt (budget covers
  frames 0-16 from an earlier snapshot, or boot.vf2snap with
  a second stop past frame 18's filing).
- Gameplay writers: round-boundary / landed-hit snapshots
  with contact (none exists in corpus -- needs driving
  two fighters together, not just held buttons).
- T5 8/10 claim on the boot chain + pump (T3).
