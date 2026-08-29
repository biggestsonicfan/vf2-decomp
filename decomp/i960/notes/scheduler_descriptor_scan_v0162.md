# v0162: scheduler descriptor scan to recurring camera task

This checkpoint continues the v0159 scheduler epilogue join at `0x00010e3c`
through the next runnable task entry.

The measured sixth-dispatch corridor begins immediately after `fa_game_info`
descriptor 13. The scheduler advances across descriptors 14, 15 and 16, all
inactive object-task records, then reaches descriptor 17 at `0x00515400`.
Descriptor 17 is active and dispatches the recurring camera task at
`0x0001d458`.

ROM-backed execution from `0x00010e3c` to `0x0001d458` is exactly:

- 62 i960 instructions;
- one procedure call (`0x00010dc8 -> 0x0001d458`);
- zero procedure returns before the task-entry boundary.

The native runtime exposes this as a dedicated `scheduler-scan` step. Admission
is deliberately narrow: current descriptor/index/scratch must identify the
measured game-info slot, the next three descriptors must remain inactive with
the measured 0x80 stride, and descriptor 17 must remain active with the
recurring-camera entry. Any changed registry layout stays fail-closed.

The implementation executes only this bounded scheduler slice and additionally
requires the exact 62-instruction budget, one-call/zero-return delta, final
instruction pointer `0x0001d458`, and final registry pointer `0x00515400`.

This creates a continuous native-runtime corridor:

`fa_game_info -> state8 high-bit boundary -> scheduler epilogue -> descriptor
scan -> recurring camera task`.

The next recovery target is to replace this bounded i960 scheduler scan with
direct C semantics or to continue the recovered recurring-camera task through
its scheduler return, whichever removes more interpreted execution per frame.

No ROM bytes or generated snapshots are committed.
