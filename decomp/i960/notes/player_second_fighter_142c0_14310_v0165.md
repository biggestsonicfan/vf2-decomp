# v0165: second-fighter 142c0 and 14310 corridors

This checkpoint advances the recovered second-fighter `fa_player` path beyond the v0164 `0x1428c` boundary without claiming the still-divergent `0x270d4`/`0x27b5c` record expanders as recovered.

For fighter 1 (`g7 = 0x00512980`), the remaining `0x1428c -> 0x142c0` aggregate record-expansion corridor is kept as a narrowly bounded direct-i960 bridge. The next two blocks are now recovered in C and ROM-validated exactly:

- `0x142c0 -> 0x14310`: `g1 == 1` appends the four-word command at `0x005502d0` instead of overwriting the existing slot at `0x005502c0`; the measured path is 55 instructions with 3 calls and 3 returns and leaves `r14 = 0`, `r15 = 0x11`.
- `0x14310 -> 0x143e4`: the second fighter takes the short no-call branch, 33 instructions and 0 calls/returns, while preserving the same memory result as the existing recovered body. The architectural post-state is recovered explicitly.

The remaining bridge now starts at `0x143e4` and is bounded by the measured reference totals: 3,765 instructions, 38 calls and 39 returns.

ROM-backed validation keeps the full fighter-1 task exact:

- entry `0x00013f08`;
- exit `0x00010dcc`;
- 16,275 instructions;
- 53 calls;
- 54 returns;
- final snapshot exactly matches the i960 reference.

The first fighter-1 `0x27b5c` invocation (`selector 0x0283`) was also probed independently and still differs from the currently recovered expander by 192 work-RAM bytes. It remains explicitly unrecovered rather than being admitted under a broadened predicate.

No ROM bytes or generated snapshots are committed.
