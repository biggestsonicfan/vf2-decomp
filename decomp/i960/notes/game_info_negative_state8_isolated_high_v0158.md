# v0158: negative state-8 isolated high-bit task boundaries

This checkpoint promotes the bilateral state-8, negative-threshold isolated
bit-14, bit-15 and bit-16 families out of the whole-task interpreted fallback.

ROM-backed controlled probes used the stable sixth-dispatch snapshot with both
fighters forced to state 8, a negative shared threshold, each isolated bit in
three measured distributions (fighter 0 only, fighter 1 only, bilateral), and
countdown 0/1. That gives 18 measured cases.

Before admission, opening the gate locally showed that the recovered task body
already matched mutable memory, execution accounting, calls and returns. The
remaining architectural state was distribution-specific task continuation:

- bit 14: bilateral `0x00010dd0`, unilateral `0x00010dd8`;
- bit 15: bilateral `0x00010dd8`, unilateral `0x00010ddc`;
- bit 16: bilateral `0x00010dd0`, unilateral `0x00010dd8`.

All three isolated families leave condition state NONE at those boundaries.
The unilateral bit-15 family additionally leaves `r3 = 0x000fffff`; the other
measured register and memory state already matches the recovered body.

The admission is intentionally narrow. It applies only when both fighter state
bytes are 8, the flag distribution is one of the measured matrix forms, the
combined state flags are exactly one of bits 14, 15 or 16, and the shared
threshold is negative. Compositions with these high bits remain fail-closed.

The next frontier is continuous recovery of the scheduler epilogue starting at
`0x00010dd0`, `0x00010dd8` or `0x00010ddc`; this checkpoint claims only the
task-return architectural boundary and does not broaden the scheduler
transition contract.

No ROM bytes or generated snapshots are committed.
