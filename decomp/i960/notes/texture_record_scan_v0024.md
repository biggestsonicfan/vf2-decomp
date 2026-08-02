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
- recovered instruction count: 43;
- memory writes: 0;
- procedure calls and returns: 0;
- final `r3`: 0;
- final `r5` and `r6`: `0x005502a8`;
- final comparison state: equal;
- final instruction pointer: `0x0004bf90`.

## C recovery

`vf2_orchestrator_scan_inactive_records` implements this path in typed C. It
requires the exact entry IP and a live procedure frame. Any active record is
rejected with `VF2_ERROR_UNSUPPORTED`; the active-record processing path is not
guessed or merged into this recovery.

`execute_texture_status_dispatch_call` now delegates its zero-count path to the
recovery and returns the `VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END` report.
The live second-dispatch bridge executes the original interpreter for exactly
43 reference instructions and compares the resulting CPU and memory snapshots
against the recovered post-state.

The implementation applies the observed i960 register, comparison-control,
instruction-counter and instruction-pointer post-state. It performs no writes
to Model 2 memory.

## Test coverage

CTest targets `vf2_orchestrator_scan` and `vf2_orchestrator_bridge` verify:

- the complete ten-record inactive scan;
- the 43-instruction accounting;
- exact `r3`, `r5`, `r6`, IP and comparison post-state;
- unchanged procedure call/return accounting;
- dispatch through the public hybrid bridge;
- rejection when an active record is encountered;
- rejection of invalid entry state;
- invalid-argument handling.

The warning-as-error build and all ROM-independent tests passed before the
functional integration commit was written.

## Claim boundary

The scan is integrated into the live hybrid bridge and contributes one of the
167 strict differential checkpoints. Together with the zero-gate recoveries,
the strict bounded-bridge accounting is now 1,268,941 recovered and 1,881
interpreted instructions.

The remaining promotion condition is a local `native-second-dispatch` run with
the supported VF2 2.1 ROM set. Until that ROM-backed run reports `MATCH`, the
function catalog remains marked `pending-rom-differential+unit`.
