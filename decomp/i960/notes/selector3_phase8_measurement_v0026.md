# Selector-3 phase-8 measured corridor (v0.0.26)

The first observed selector-3 phase-8 invocation (`0x0000b9b8`) uses the nonterminal countdown path. Starting from the real post-phase-7 memory state with the framed CPU entry restored at `0x0000a6c0`, ROM execution returns normally to `0x0000a010` after **37 instructions / 3 calls / 4 returns**.

Observed calls are `0xa6c0 -> 0xacf8 -> 0xb9b8 -> 0x1344`. The recovered phase-8 handler already produced the correct memory effects for this path; the remaining mismatch was only wrapper accounting and CPU poststate. The old dispatcher fell through to the generic selector cleanup and reported `260 / 1 / 2`.

Commit `4a14b953a233bd956a67bf6ec15b03331af08204` gives the nonterminal phase-8 path its measured accounting, preserves selector 3, sets `g0 = -1`, restores the ROM signed comparison result, and returns before generic selector cleanup.

The task pointer published at `0x00500834` is `0x00515b00` in this controlled state and its phase-8 countdown is the 32-bit word at `0x00515b50`, initially `320`. Repeated ROM invocations confirm it decrements by exactly one on the nonterminal path. The real post-phase-7 state has `0x00550000 == 1`, so a zero countdown takes the ready-return branch.

The alternate zero-counter/not-ready branch was isolated by keeping the full real post-phase-7 state and changing only those two branch inputs (`0x00515b50 = 0`, `0x00550000 = 0`). ROM reaches the inline text thunk at `0x00009444` after **26 instructions**, with two nested procedure calls and zero returns relative to the framed `0xa6c0` snapshot. The recovered handoff already matched IP, live frames, registers, counters and all modeled memory; its only remaining difference was the condition code (`EQUAL` instead of ROM `GREATER`). Commit `30654df3ac8918c25b9652fb6399c09d5c80d220` restores the ROM signed-comparison poststate.

Both phase-8 branches require full native-vs-ROM snapshot equality before the phase is considered closed.
