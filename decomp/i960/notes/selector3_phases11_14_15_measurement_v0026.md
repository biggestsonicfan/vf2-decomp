# Selector-3 phases 11, 14 and 15 measured corridors (v0.0.26)

Controlled ROM runs from the framed `0x0000a6c0` entry, using the real post-phase-7 machine state with only the phase byte changed, measured the following selector-3 corridors:

- phase 11 (`0x0000bcb0`): **34 instructions / 3 calls / 4 returns**;
- phase 14: **37 instructions / 3 calls / 4 returns**;
- phase 15: **48 instructions / 3 calls / 4 returns**.

For all three phases the pre-existing recovered handler already matched every modeled memory region. The only mismatches were the shared generic selector cleanup accounting plus `g0` and comparison poststate.

Commit `8c3784ef3b9402812563f18e03d610c5af841c25` gives each phase its individually measured instruction count, restores `g0 = -1` and the ROM `GREATER` comparison state, accounts three nested calls/returns plus the outer return, and exits before generic cleanup.

Phase 12 is deliberately excluded from this batch: although its total instruction count matches phase 6, ROM leaves additional live `0x8f1c` register poststate that must be reconstructed semantically rather than hidden behind accounting.
