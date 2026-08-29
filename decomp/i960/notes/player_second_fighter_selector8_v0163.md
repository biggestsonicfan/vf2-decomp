# v0163: second-fighter player selector-8 corridor

This checkpoint closes the measured second-player corridor that starts at
`fa_player` for fighter 1 (`0x00512980`) and previously rejected selector 8.

Two interpreter-boundary defects were isolated first:

- `0x00016504 -> 0x00014418` executes successfully as 1,690 direct i960
  steps, with 16 calls and 17 returns. The generic `vf2_i960_run` helper
  rejected this corridor, so the bridge is now explicitly bounded by the
  measured entry, exit, step count and call/return deltas.
- the fighter-1 prefix reaches the selector-8 branch at `0x00014278`, calls
  `0x0001b74c`, then `0x000094d0`, updates the RNG word at `0x00500098`, and
  indexes the 16-bit table at `0x0201cb8c`.

For the measured fighter-1 entry the prefix reaches `0x00014288` after 872
instructions, two calls and two returns. A ROM snapshot and the recovered-C
snapshot match exactly at that boundary, including architectural CPU state,
mutable memory and diagnostic counters. The selector helper returns
`g0 = 0x00000284` in this state.

The checkpoint also corrects `0x0050a0b6` to receive the ROM's zero byte in
the prefix instead of accidentally inheriting the fighter selector value.

The remaining fighter-1 body from `0x00014288` to scheduler return is still an
explicit ROM bridge. The generic run helper is not reliable for this measured
corridor, so this one bridge is executed step-by-step and guarded by the exact
reference totals: 15,403 instructions, 51 calls and 52 returns.

End-to-end ROM-backed validation for fighter 1 now gives:

- entry: `0x00013f08`;
- exit: `0x00010dcc`;
- instructions: 16,275;
- calls: 53;
- returns: 54;
- final snapshot: exact match against the i960 reference.

This is a continuity checkpoint, not a claim that the post-`0x14288` player
body is decompiled. The next recovery target is to replace that long bounded
bridge with recovered C while preserving the now-proved player-task boundary.

No ROM bytes or generated snapshots are committed.
