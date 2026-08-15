# Selector-3 phase-9 measured corridor (v0.0.26)

The controlled selector-3 phase-9 invocation (`0x0000baec`) was measured from the framed `0x0000a6c0` entry using the real post-phase-7 machine state with only the phase byte changed to 9.

ROM execution returns normally to `0x0000a010` after **34 instructions / 3 calls / 4 returns**. The observed calls are `0xa6c0 -> 0xacf8 -> 0xbaec -> 0x1344`.

The existing recovered phase-9 handler already matched every modeled memory region. Its only mismatch was falling through to the generic selector cleanup (`260 / 1 / 2`) and therefore leaving the wrong CPU accounting, `g0`, and condition state.

Commit `43fab0ef3a171fa082564b9f846d8dc1ae78e087` gives phase 9 its measured accounting, sets `g0 = -1`, restores the ROM `GREATER` comparison result, and returns before generic cleanup.

Full native-vs-ROM snapshot equality remains the acceptance criterion for closing the phase.
