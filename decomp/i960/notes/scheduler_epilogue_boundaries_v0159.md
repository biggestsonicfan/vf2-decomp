# v0159: scheduler epilogue boundary bridge

This checkpoint makes the three task-return boundaries recovered in v0158
executable by the native runtime instead of stopping at the game-info task
boundary.

The i960 epilogue is one shared sequence with three measured entry points:

- `0x00010dd0` executes 14 instructions to `0x00010e3c`;
- `0x00010dd8` executes 13 instructions to `0x00010e3c`;
- `0x00010ddc` executes 12 instructions to `0x00010e3c`.

The sequence performs the timer-derived scratch update, current-index/runtime
flag checks, and the current scratch accumulator update before converging at
`0x00010e3c`. No procedure calls or returns occur inside this measured slice.

The runtime exposes the slice as a dedicated `scheduler-epilogue` step kind.
Execution is deliberately fail-closed: only the three measured entry addresses
are accepted, each entry has an exact instruction budget, the exit must be
`0x00010e3c`, and any unexpected call/return rejects the transition.

ROM-backed validation used representative v0158 task exits for:

- bilateral negative state-8 isolated bit 14 (`0x10dd0`);
- unilateral negative state-8 isolated bit 14 (`0x10dd8`);
- unilateral negative state-8 isolated bit 15 (`0x10ddc`).

All three native continuation snapshots match the i960 reference exactly at
`0x00010e3c`, including CPU state, mutable memory, registers and instruction
accounting. The architectural compare state at the join is GREATER.

This checkpoint intentionally stops at the descriptor-scan join. The next
frontier is recovery of `0x00010e3c -> 0x00010d94/0x00010e58` descriptor
iteration through entry of the next runnable task.

No ROM bytes or generated snapshots are committed.
