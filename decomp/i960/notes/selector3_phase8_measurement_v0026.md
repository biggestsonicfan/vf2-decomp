# Selector-3 phase-8 measured corridor (v0.0.26)

The first observed selector-3 phase-8 invocation (`0x0000b9b8`) uses the nonterminal countdown path. Starting from the real post-phase-7 memory state with the framed CPU entry restored at `0x0000a6c0`, ROM execution returns normally to `0x0000a010` after **37 instructions / 3 calls / 4 returns**.

Observed calls are `0xa6c0 -> 0xacf8 -> 0xb9b8 -> 0x1344`. The recovered phase-8 handler already produced the correct memory effects for this path; the remaining mismatch was only wrapper accounting and CPU poststate. The old dispatcher fell through to the generic selector cleanup and reported `260 / 1 / 2`.

Commit `4a14b953a233bd956a67bf6ec15b03331af08204` gives the nonterminal phase-8 path its measured accounting, preserves selector 3, sets `g0 = -1`, restores the ROM signed comparison result, and returns before generic selector cleanup.

The zero-counter/not-ready terminal phase-8 handoff to the inline text thunk at `0x00009444` is a separate observed path and remains to be validated independently.
