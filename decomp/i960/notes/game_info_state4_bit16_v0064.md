# fa_game_info: exact state-4 + bit-16 native corridor (v0064)

## Scope

Recovered the isolated fighter-state-4 + state-bit-16 dispatcher corridor where either fighter has `fighter+0xa00 == 4`, bit 16 set in `fighter+0x1a4`, and bits 6/14/15 clear.

## Semantic fix

The ROM does not behave like the state4+bit14 corridor here: state4+bit16 leaves state bit 11 set at the `0x184ec` rejoin. The native recovery now preserves the observed `0x00010200 -> 0x00010a00` state transition.

## Accounting

Measured ROM deltas before correction were:

- fighter 0 state4+bit16: -4 instructions
- fighter 1 state4+bit16: -4 instructions
- both fighters state4+bit16: -7 instructions

This decomposes exactly into +3 instructions per state4+bit16 fighter plus +1 fixed dispatcher instruction.

## Validation

Using the real VF2 v2.2 ROM state:

- fighter 0 state4+bit16 only: exact through `0x10dcc`
- fighter 1 state4+bit16 only: exact through `0x10dcc`
- both fighters state4+bit16: exact through `0x10dcc`

For all three cases:

- architectural snapshot: exact
- mutable memory: exact
- instructions: delta 0
- calls: delta 0
- returns: delta 0
- interrupt entries: delta 0
- interrupt returns: delta 0

Clean source candidate: `043dab33197a499922c220464051400f5094fd9a`.
