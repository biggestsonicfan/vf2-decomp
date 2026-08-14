# v0.0.24 texture record advance

## Observed path

The active texture-record loop reaches `0x0004bf60` four times. On every
visit, `r6` is one and the signed halfword at record offset `0x02` is positive.
The block decrements the remaining count, advances the two stream pointers by
four bytes, writes all three values back to the record, and branches to
`0x0004bd24`.

The exact observed path is 12 instructions per visit, for 48 recovered
instructions across four visits. It writes 10 bytes per visit and performs no
procedure calls or returns. Unsupported counter or loop states are rejected.

## Comparison-state contract

The original isolated bridge fixture was built without a ROM attached and
models `cmpdeco 1, r6, r6` as leaving the helper with an equal comparison
state. That remains the synthetic unit-test contract.

Continuous ROM-backed differential replay now reaches this boundary through
the recovered texture checkpoint/resume corridor. At the `0x0004bf60 ->
0x0004bd24` boundary the reference CPU carries `LESS` (`compare_result == 1`,
arithmetic-control condition bits `0b100`), while the isolated helper would
otherwise return `EQUAL` (`compare_result == 2`, condition bits `0b010`).

`texture_bridge_condition.c` therefore applies the `LESS` preservation fix
only when a main ROM is attached. This keeps the isolated semantic fixture
stable while making the ROM-backed bridge reproduce the live architectural
post-state exactly.

The following all-inactive status scan from `0x0004bd24` to `0x0004bf90`
has the complementary live contract: the ROM reference reaches final-status
with `EQUAL`. The bridge explicitly restores that condition at the scan-end
boundary so it is also the value saved by the next interrupt frame.

## Validation

The public isolated bridge test still covers the helper CPU and memory
post-state, including `g0`, `r6`, `r8`, `r9`, `r10`, the synthetic equal
comparison state, the halfword count, and both 32-bit pointers.

The ROM-backed `compare-texture-bridge` corridor is the preservation oracle
for the live path. It compares the reference i960 interpreter against the
recovered bridge at every covered checkpoint, including CPU comparison state
and Model 2 memory. The record-advance condition correction was accepted only
after a local binary experiment changing no other state moved the first
mismatch forward to the status-scan boundary.
