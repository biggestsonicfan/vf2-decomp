# Selector-3 phase-10 measured corridor (v0.0.26)

The controlled selector-3 phase-10 invocation (`0x0000bc10`) was measured from the framed `0x0000a6c0` entry using the real post-phase-7 machine state with only the phase byte changed to 10.

ROM execution returns normally to `0x0000a010` after **39 instructions / 3 calls / 4 returns**. The observed calls are `0xa6c0 -> 0xacf8 -> 0xbc10 -> 0x1344`.

The existing recovered phase-10 handler already matched every modeled memory region. The remaining mismatch was generic selector cleanup accounting and CPU poststate.

Commit `b162deb53e947f45b35db18aa6d0979472b687c8` gives phase 10 its measured accounting, sets `g0 = -1`, restores the ROM `GREATER` comparison result, and returns before generic cleanup.

Full native-vs-ROM snapshot equality remains the acceptance criterion for closing the phase.
