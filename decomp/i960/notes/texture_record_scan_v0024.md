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

`execute_texture_status_dispatch_call` delegates its zero-count path to the
recovery and returns `VF2_HYBRID_BRIDGE_TEXTURE_STATUS_SCAN_END`. The live
second-dispatch bridge executes the original interpreter for exactly 43
reference instructions and compares the resulting CPU and memory snapshots
against the recovered post-state.

## Validation

CTest targets `vf2_orchestrator_scan` and `vf2_orchestrator_bridge` passed in a
warning-as-error build. The exact supported VF2 2.1 ROM set then produced:

```text
Native second-dispatch validation: MATCH
Final CPU and memory state:         MATCH
```

The scan contributes one of the 168 confirmed differential checkpoints. The
function catalog is therefore promoted to `dynamic-differential+unit`.
