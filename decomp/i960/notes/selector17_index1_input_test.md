# Selector 17 bit-7 index 1: input test

A controlled phase-17 transition uses `0x00500704 = 0x00001004` at the
main-final boundary: bit 12 advances phase index 0 to 1 and bit 2 immediately
re-arms the diagnostic bit. The resulting state is `0x005000a4 = 0x81`,
`0x005000a5 = 0`, and `0x005000a6 = 0xff`.

The primary bit-7 table entry at `0x0005feb0` points to `0x00059718`.
`0x00059718` uses `0x005000a5` as a secondary selector; the measured first
visit (`a5 = 0`) selects `0x00059738`.

This body is the INPUT TEST screen. In the measured state it displays:

- UP, DOWN, RIGHT, LEFT, PUNCH, KICK and GUARD as `ON` for both players;
- START as `OFF` for both players;
- COIN CHUTE 1, COIN CHUTE 2, SERVICE SW and TEST SW as `OFF`;
- `PUSH TEST BUTTON TO EXIT`.

Reference execution from `0x0000a6c0` to `0x0000a010` consumes 3,316 i960
instructions, 51 procedure calls and 52 procedure returns. The first visit
sets `0x005000a5 = 1`; apart from the input-test tile output, only the observed
stack spill bytes are normalized to `00 00 56`.

The tile reproduction follows the ROM's exact write runs rather than treating
visually blank gaps as written spaces. This matters because the diagnostic text
helpers write some spaces as `0x8020` while leaving other existing `0x0020`
cells untouched; both render identically but are distinct in strict snapshot
comparison.

The second visit (`a5 = 1`) enters the body at `0x0005977c`. With
`0x00500708 = 0`, the already rendered INPUT TEST screen is left untouched and
the measured path from `0x0000a6c0` to `0x0000a010` consumes 1,622 i960
instructions, 37 procedure calls and 38 procedure returns; only the observed
`00 00 56` stack-spill bytes are normalized.

A released TEST bit does not exit on that same second visit. With
`0x00500708 = 0x00000004`, `0x0005977c` performs the same input-test refresh,
then promotes the secondary selector from `a5 = 1` to `a5 = 2`. This measured
transition consumes 1,624 instructions with the same 37 calls and 38 returns;
relative to the no-release second visit, the only additional architectural
change is `0x005000a5 = 2`.

The actual exit is therefore deferred to the third visit: `a5 = 2` selects
`0x000597a8`, which refreshes the INPUT TEST once more and tests
`0x00500708 & 0x04000004` before branching to the shared diagnostic teardown at
`0x0005f140`.

The recovered implementation remains restricted to measured input states:
first visit uses `0x00500700 = 0x0ff7f700`, `0x00500704 = 0`, matching previous
input and selector mask `0x00020000`; second-visit recovery accepts released
flags zero or the measured TEST-release value `0x4`. Other input-state variants
and the third-visit teardown remain separate recovery cases.
