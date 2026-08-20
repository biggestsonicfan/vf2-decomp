# fa_game_info: exact neutral state-4 native corridor (v0062)

## Scope

Recovered the remaining neutral fighter-state-4 dispatcher corridor where either fighter has `fighter+0xa00 == 4` and state flags at `+0x1a4` have bits 6/14/15/16 clear.

## Concrete fixes

1. `0x18644` zero/zero state now preserves the ROM's final `GREATER` compare result.
2. Corrected a decompilation error at `0x1850c`: the ROM uses `LDA 0x1f8(g7), r5`, so `fighter+0x1f8` is the address of the inline float table, not a pointer loaded from that field.
3. Recovered the observed bit-7-set early-return corridor in nested helper `0x17b68`, ending at the `RET` at `0x17c1c`; unobserved branches remain fail-closed.
4. Corrected state-4 instruction accounting: four extra instructions before rejoining `0x18188`, plus the measured neutral-state-4 dispatcher instruction.

## Validation

Using the real VF2 v2.2 ROM state and three controlled variants:

- fighter 0 state 4 only: exact through `0x10dcc`
- fighter 1 state 4 only: exact through `0x10dcc`
- both fighters state 4: exact through `0x10dcc`

For all three cases:

- architectural snapshot: exact
- mutable memory: exact
- instructions: delta 0
- calls: delta 0
- returns: delta 0
- interrupt entries: delta 0
- interrupt returns: delta 0

Clean source candidate: `074ff4f45666a9ef6699615b90aefaad2c954d20`.
