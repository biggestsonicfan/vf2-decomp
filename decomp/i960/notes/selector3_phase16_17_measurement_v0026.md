# Selector-3 phases 16 and 17 measured corridors (v0.0.26)

The selector-3 phase table at `0x0000aac4` has exactly eighteen entries
(phases 0-17, ending at `0x0000ab00`); phase 16 dispatches to `0x0000c414`
and phase 17 to `0x0000c448`. These are the final two selector-3 phase
bodies, and both are now recovered.

## Body disassembly

Phase 16 (`0x0000c414`, seven instructions) decrements the 32-bit countdown
at `[0x00500834] + 0x50`, returns while the result is nonzero, and on zero
takes a three-instruction epilogue that increments the phase byte at
`0x00500030`:

```
0000c414  ld       0x00500834, r3
0000c41c  ld       0x00000050(r3), r15
0000c420  subo     1, r15, r15
0000c424  st       r15, 0x00000050(r3)
0000c428  cmpobe   0, r15, 0x0000c430
0000c42c  ret
0000c430  ldib     0x00500030, r15
0000c438  lda      0x00000001(r15), r15
0000c43c  stib     r15, 0x00500030
0000c444  ret
```

Phase 15's terminal transition arms this countdown with 128, so the natural
corridor holds phase 16 for 129 ticks before advancing.

Phase 17 (`0x0000c448`, three instructions) clears the phase byte to zero,
wrapping the selector-3 phase cycle back to phase 0:

```
0000c448  mov      0, r15
0000c44c  stib     r15, 0x00500030
0000c454  ret
```

## Controlled measurement

The corridors were measured with the controlled differential harness used
for the earlier phase notes: the reference i960 was booted from cold reset
with the standard frame/timer interrupt model and captured at the first
natural frame-dispatch entry `0x0000a6c0` (5,579,856 instructions in, task
pointer `0x00500834 -> 0x00515b00` matching the published post-phase-7
state). Only the selector byte, the phase byte and the countdown word were
changed per scenario. The frame-dispatch corridor ends with the
`0xae30 -> 0xa6f4` ret chain landing at the scheduler call site
`0x0000a010`.

Harness calibration reproduced the published corridors exactly before the
new phases were measured: phase 8 nonterminal (countdown 320) is
**37 instructions / 3 calls / 4 returns** and phase 11 is **34 / 3 / 4**.

The new measurements:

- phase 16 stay (countdown `2 -> 1`): **34 instructions / 3 calls /
  4 returns**, phase byte unchanged;
- phase 16 advance (countdown `1 -> 0`): **37 instructions / 3 calls /
  4 returns**, phase byte `16 -> 17`;
- phase 17 reset: **31 instructions / 3 calls / 4 returns**, phase byte
  `17 -> 0`.

The counts are internally consistent: the stay body is three instructions
shorter than the advance epilogue, and the phase-17 body is three
instructions shorter than the stay body, over the same 28-instruction
wrapper overhead.

## Differential validation

Controlled native-versus-reference runs from the same snapshot and writes
match exactly for all three corridors, including the phase-8 control:
identical instruction/call/return accounting, complete CPU register and
condition equality, and every mutable Model 2 memory region byte-equal at
the `0x0000a010` boundary.

## Post-boot input-profile condition poststate fix

Re-running the differential also isolated a pre-existing master regression
that this session fixed: the post-boot input-profile differential
(`vf2_post_boot_input_profile_tests`) had been failing since the condition
preservation work refined the reference executor. The reference now models
compare-branch and bit-branch condition effects, and three recovered blocks
left stale state:

- `execute_post_boot_input_profile_entry` never set a condition; it now
  tracks the last executed `cmpob*`/bit test on the taken path (the bit
  tests leave EQUAL for a set tested bit and NONE for a clear one);
- `execute_post_boot_float_defaults_init` hard-coded EQUAL; it now
  reproduces the `0x1ff0c` closing compares (EQUAL on the taken mode-6/10
  branches, `compare(6, mode)` on the ordinary fall-through);
- `execute_post_boot_input_profile_load` never set a condition; it now
  reproduces the closing `cmpobne 4, profile`.

All seven controlled profile cases match again at every one of their three
block boundaries, and the full 49-target local CTest suite is green.
