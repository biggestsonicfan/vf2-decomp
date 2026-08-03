# Native differential lockstep

## Purpose

`vf2_native_differential_run_until` is the reusable validation layer between
recovered C execution and the reference i960 executor. It removes the need for
each developer command to maintain its own list of recoverable instruction
pointers and its own block-by-block comparison loop.

The API is declared in `include/vf2/native_differential.h` and implemented in
`src/recovered/native_differential.c`.

## Contract

The caller supplies two independently allocated Model 2 machines and CPUs that
start from the same architectural and mutable-memory snapshot:

- the **reference** side executes original i960 instructions;
- the **native** side executes only `vf2_native_runtime_step` recoveries;
- native interpreter fallback is never permitted.

For every accepted native block, the runner:

1. records the `vf2_native_runtime_step_report`;
2. advances the reference i960 by exactly the recovered instruction count;
3. verifies that the reference instruction counter advanced by that count;
4. captures complete snapshots of both CPUs and all mutable Model 2 memory;
5. compares the snapshots; and
6. continues only when the states are identical.

A native unsupported path, reference execution failure, CPU or memory mismatch,
or block-budget exhaustion returns immediately with a partial
`vf2_native_differential_report`. The report preserves the last native step,
final addresses, cumulative instruction counts and the first snapshot
difference.

## Why this advances v0.1.0

The existing `vf2i960 native-second-dispatch` path contains a large private
candidate router and comparison loop. The v0.1.0 acceptance target instead
requires the production `vf2_native_runtime` dispatcher to drive the recovered
side through another complete frame and scheduler sweep.

The new runner provides that missing reusable primitive. The next integration
step is to initialize synchronized reference/native snapshots at the validated
second `fa_game_info` entry, then invoke:

```c
vf2_native_differential_run_until(
    &reference_machine,
    &reference_cpu,
    &native_machine,
    &native_cpu,
    &native_state,
    UINT32_C(0x0001645c),
    repeated_frame_block_budget,
    &report
);
```

The stop address alone is not sufficient because the starting address is also
`0x0001645c`; the command must first execute the current task body or expose a
minimum-block/stop-occurrence policy. That integration detail should be solved
in the CLI wrapper rather than by weakening the runner's zero-length-run
contract.

## Tests

`tests/recovered/test_native_differential.c` covers:

- invalid arguments;
- a synchronized zero-length run;
- initial instruction-pointer divergence;
- explicit block-budget exhaustion; and
- propagation of an unsupported native runtime address.

ROM-backed positive lockstep coverage belongs in the forthcoming
`native-third-dispatch` command, where the full supported ROM state is
available.
