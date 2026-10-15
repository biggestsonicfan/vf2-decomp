# E1c: 0xed8 key-on arm recovered -- first completed dispatch (9/10 arc)

Status: `sound_board_run_engine_keyon_ed8` + full-dispatch
differential green in-file (whole `vf2_m68k_cdispatch_tests`
binary, 0 FAIL). Full suite in the commit validation.

## Recovery

15 insns at 0xed8-0xf10, called from the engine head's
bit31-taken branch: key = (table+0 & 0xfffff) | 0x10300000 to
SCSP[slot+0] (u16 pair); slot+4 = table+8 low word; slot+6 =
table+4 low word minus 1 (subq.w wrap exact); RMW slot+0 high
byte |= 0x18 through the model (read-OR-write, so
`synchronize_slot` fires -- KEYONB sets, slot_active 1);
stream byte ram[0x2000+slot] = 0x40. Returns VF2_OK (the rts
resumes the drain). Per-op OOB pins (0xed8/0xee4/0xef0/0xefa).

## The at_door fix (load-bearing)

The C completed the arm but `dispatch_next` still held the
packet: `at_door` (Loop-B traversed, set by the HIGH table
path) conflated traversal with refusal. No completed-through-
Loop-B path existed before, so OK+door=1 was impossible and
the hold keyed on `status != OK || at_door`. Changed to hold
on status alone -- behavior-identical for every existing path
(full suite proves), and the E1c shape now consumes (count 0,
cursor +4, handled 1). Handler on the OK path stays 0x858
(class entry, gate-hit precedent).

## Differential (S4 + staged bit31)

- C: OK, handled 1, count 0, cursor 0->4, scan=20 match=1
  (all 20 descriptors scanned past the rts, like silicon),
  sword 0x1004.
- S: 0xf10 in 381 steps (d2 0x1004, d4 0, d5 0x4a18);
  slot0 0x1830/0x0000, +4 0x0000, +6 0x4a61; stream 0x40;
  active0=1; rts -> 0x7f6 in 1 step; drain exit 0x265c
  in 62 steps.
- Full RAM under the S4 exclusions + full SCSP incl.
  slot_active match.
- Render peak 4251 (>0, pinned) + determinism -- first
  audible render from a native dispatch (S4b manual keying
  still stands for the fall-through shape).

## Debugging trail (kept for the pattern)

The C first failed as UNSUPPORTED with a stale 0x858 handler
-- NOT a logic error but the at_door hold firing on a
completed path. Breadcrumbing via handler_address
(0xc1e/0xc68/table markers, all removed) + report-counter
prints (sword=0x1004 proved the picker ran) localized it in
three rebuilds. Permanent yield: per-op OOB pins in ed8.

## Still open

- 0xd92 arm: refused (dead copy loop at 0xdf8, same fate).
- Fall-through (E1b) still refuses at 0xcb2 by design.
- a0 body, gameplay writers: unchanged.
