# fa_game_info: exact state-4 + bit-6 native corridor (v0065)

## Scope

Recovered the isolated `fighter_state == 4` plus state-flag bit 6 corridor of `fa_game_info`, with bits 14/15/16 clear and the shared fighter threshold non-negative.

## Accounting

The semantic path already matched the ROM. Differential measurement showed the native path over-counted instructions by:

- fighter 0 only: +3
- fighter 1 only: +3
- both fighters: +7

This decomposes into three instructions per state4+bit6 fighter plus one additional bilateral instruction. The recovered accounting therefore uses a one-instruction state4/bit6 prefix instead of the ordinary four-instruction state4 prefix, plus a one-instruction bilateral correction when both fighters are state4+bit6.

## Validation

Using the real VF2 v2.2 ROM state and three controlled variants:

- fighter 0 state4+bit6 only: exact through `0x10dcc`
- fighter 1 state4+bit6 only: exact through `0x10dcc`
- both fighters state4+bit6: exact through `0x10dcc`

For all three cases:

- architectural snapshot: exact
- mutable memory: exact
- instructions: delta 0
- calls: delta 0
- returns: delta 0
- interrupt entries: delta 0
- interrupt returns: delta 0

Clean source candidate: `5502cb9243aeeb818544c0651cd4e091512e8646`.
