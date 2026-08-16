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

The second visit (`a5 = 1`) enters the alternate body behind `0x00059718`.
With `0x00500708 = 0` no input release/exit condition fires and the already
rendered INPUT TEST screen is left untouched. The measured path from
`0x0000a6c0` to `0x0000a010` consumes 1,622 i960 instructions, 37 procedure
calls and 38 procedure returns; only the observed `00 00 56` stack-spill bytes
are normalized. This case is kept separate from the TEST-button exit, whose
branch tests `0x00500708 & 0x04000004`.

The recovered implementation is intentionally restricted to measured input
states: first visit uses `0x00500700 = 0x0ff7f700`, `0x00500704 = 0`, matching
previous input and selector mask `0x00020000`; the second visit additionally
requires released flags to remain zero. Input-state variants and the later
TEST-button exit remain separate recovery cases.
