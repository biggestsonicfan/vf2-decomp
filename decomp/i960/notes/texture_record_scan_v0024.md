# v0.0.24 inactive texture-record scan

## Scope

The observed final pass through the texture-record table begins at
`0x0004bd24`. Unlike the four earlier visits, this invocation finds no active
record. It scans the complete table and branches to the final status gate at
`0x0004bf90`.

The accepted path has these fixed properties:

- record range: `0x00550168` through `0x005502a8`;
- record stride: `0x20` bytes;
- active-count field: signed halfword at record offset `0x02`;
- records visited: 10;
- active records: 0;
- interpreted instruction equivalent: 43;
- memory writes: 0;
- procedure calls and returns: 0;
- final `r3`: 0;
- final `r5` and `r6`: `0x005502a8`;
- final comparison state: equal;
- final instruction pointer: `0x0004bf90`.

## C recovery candidate

`vf2_orchestrator_scan_inactive_records` implements this path in typed C. It
requires the exact entry IP and a live procedure frame. Any active record is
rejected with `VF2_ERROR_UNSUPPORTED`; the active-record processing path is not
guessed or merged into this candidate.

The implementation applies the observed i960 register, comparison-control,
instruction-counter and instruction-pointer post-state. It performs no writes
to Model 2 memory.

## Unit coverage

CTest target `vf2_orchestrator_scan` verifies:

- the complete ten-record inactive scan;
- the 43-instruction accounting;
- exact `r3`, `r5`, `r6`, IP and comparison post-state;
- unchanged procedure call/return accounting;
- rejection when an active record is encountered;
- rejection of invalid entry state;
- invalid-argument handling.

## Claim boundary

This is an evidence-backed semantic candidate, not yet a live hybrid bridge
block. Therefore the strict validator remains at 1,268,866 recovered and 1,956
interpreted bridge instructions. Once the dispatcher invokes this candidate and
full CPU/memory differential comparison passes, the expected totals become
1,268,909 recovered and 1,913 interpreted instructions.
