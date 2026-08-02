# v0.0.24 texture counter update

## Observed path

After the post-body maintenance call returns to `0x0004bb98`, the
orchestrator examines three 32-bit counters:

- `0x005502c0` is zero, so the decremented register value is discarded;
- `0x005502d0` is zero, so the decremented register value is discarded;
- `0x005502e0` is greater than one, so it is decremented and written back.

The block reaches `0x0004bc58` after exactly 14 instructions. It performs one
32-bit memory write and no procedure calls or returns.

## i960 semantics

`cmpdeco 1, r4, r4` compares the literal one with the original value of `r4`
using an unsigned comparison, then stores `r4 - 1` in the destination. The
final observed comparison is `less`, because one is less than the third
counter's original value.

## Validation

The public bridge test verifies the instruction count, final IP, `r3`, `r4`,
comparison and arithmetic-control state, unchanged first two counters, and the
single decrement of the third counter.

The warning-as-error build and all five ROM-independent orchestrator tests
passed. The exact supported VF2 2.1 ROM set then produced:

```text
Native second-dispatch validation: MATCH
Final CPU and memory state:         MATCH
```

The block is one of the 168 confirmed differential checkpoints and is promoted
to `dynamic-differential+unit`. Confirmed bridge accounting is 1,268,955
recovered and 1,867 interpreted instructions.
