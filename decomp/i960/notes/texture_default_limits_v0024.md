# v0.0.24 texture default-limit candidate

## Scope

This note records the first semantic candidate extracted from the remaining
texture-orchestrator path. It does not claim a recovered bridge block yet.

The candidate begins at `0x0004bfe0`. On the observed startup invocation, the
i960 path reads runtime state, selects the default branch class and writes:

- `0x00003e80` to work RAM `0x00550004`;
- `0x00004e20` to work RAM `0x00550008`.

The semantic implementation is split into:

- `vf2_orchestrator_select_default_limits`, a pure decision helper;
- `vf2_orchestrator_apply_default_limits`, a bounded Model 2A memory adapter.

Both live under `src/analysis`, rather than `src/recovered`, until the function
is connected to the native bridge runner and checked against the interpreted
CPU/memory post-state.

## Accepted branch class

The current candidate accepts the default path only when:

- runtime flag bit 16 at `0x00500068` is clear;
- the display-mode byte at `0x0050002b` does not select any alternate branch
  observed in the disassembly around `0x0004bfe0`.

Modes selecting alternate paths return `VF2_ERROR_UNSUPPORTED`. No values are
written when the branch is unsupported.

## Unit evidence

`tests/analysis/test_orchestrator_limits.c` verifies:

- accepted and rejected display-mode classes;
- rejection of runtime flag bit 16;
- exact default output constants;
- exact destination addresses and eight written bytes;
- preservation of destination values when a branch is unsupported;
- invalid-argument handling.

The CMake target is `vf2_orchestrator_limits_tests`, registered as CTest
`vf2_orchestrator_limits`.

## Claim boundary and next proof

The current v0.0.23 CSV proves the executed instructions and output writes but
does not contain the complete entry-register and input-memory snapshot required
for final acceptance. Therefore:

- no new `vf2_hybrid_bridge_kind` has been added;
- the hard-coded native bridge totals remain unchanged;
- the candidate is not counted among recovered instructions or checkpoints.

The next step is to capture the live entry state at `0x0004bfe0`, invoke the
candidate from the native bridge dispatcher, execute the same instruction count
on the reference machine, and compare complete CPU, local-frame and mutable
memory state before moving this function to `src/recovered`.
