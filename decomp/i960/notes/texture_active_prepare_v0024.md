# v0.0.24 texture active-record preparation

## Observed path

At `0x0004bde0`, the orchestrator loads the active record flags, remaining
count, and two stream pointers. The observed flags have bits 3 and 4 clear. It
reads the current stream word, derives an index into the signed coordinate
table at `0x0004c120`, adds the packed high-byte offsets, and calls the already
recovered address-table helper at `0x0004d16c`.

The path executes 22 instructions and one call per visit. It occurs four times,
recovering 88 instructions and four procedure calls. One 32-bit active-flags
word is written per visit. Unsupported flag/count branches remain rejected.

## Validation

The bridge test uses a synthetic ROM table and verifies the saved caller frame,
all affected global registers, the active-flags write, instruction/call
accounting, and the exact child target and return address. The block preserves
the incoming comparison and arithmetic-control state because its BBS/BBC
instructions are direct COBR comparisons.

The exact VF2 2.1 ROM-backed `native-second-dispatch` validator executed 22
reference instructions for each of the four visits and reached complete CPU
and Model 2 memory `MATCH`.

The strict totals are now 1,269,091 recovered and 1,731 interpreted
instructions across 176 recovered blocks and memory checkpoints. Recovered
calls and returns are 270 / 300.
