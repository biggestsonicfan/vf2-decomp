# v0164: recover second-fighter 0x19ef8 corridor

This checkpoint removes the first 1,805 instructions from the bounded ROM continuation introduced for fighter 1 in v0163.

The recovered `0x00019ef8` C path previously admitted only selector `0x505` (the first fighter). ROM-backed measurement of fighter 1 shows that the same structural path is used with selector `0x284`, with one profile-specific record signature: source bytes 4..7 are `00 00 7f 00`.

For selector `0x284` the measured path also differs in four explicit outputs: `player + 0x170` receives `0x40000000`; the record cursor stored at `player + 0x6d0` and `player + 0x82c` is `data_pointer + 11` rather than `+12`; `player + 0xbdc` receives byte `0x20`; and architectural `g2`, return register and compare state differ at the join.

The recovered path reaches `0x0001428c` after exactly 1,805 instructions, four calls and four returns. The resulting snapshot is an exact match against the i960 reference, including CPU state, mutable memory and diagnostic counters.

The remaining fighter-1 continuation now starts at `0x0001428c` instead of `0x00014288`. It is still an explicit bounded i960 bridge and is guarded by the measured totals of 13,598 instructions, 47 calls and 48 returns.

End-to-end fighter-1 validation remains exact: entry `0x00013f08`, exit `0x00010dcc`, 16,275 instructions, 53 calls, 54 returns, with the final snapshot matching the ROM reference exactly.

No ROM bytes or generated snapshots are committed.
