# fa_game_info: exact state-4 bit14+bit16 combined corridor (v0067)

## Scope

Recovered the first mixed conditional state-4 corridor where either fighter has `fighter+0xa00 == 4` and both state bits 14 and 16 are set, with bits 6 and 15 clear.

## Result

No new child semantics were required. The already recovered state-4 bit14/bit16 bodies produce the exact ROM state when this combined corridor is admitted natively.

The only measured interaction was bilateral accounting: when both fighters are simultaneously in state4+bit14+bit16, the native path had one extra recovered instruction. Subtracting that single shared instruction makes the full path exact.

## Validation

Using the real VF2 v2.2 ROM state and three controlled variants:

- fighter 0 state4+bit14+bit16: 902 ROM / 902 native instructions
- fighter 1 state4+bit14+bit16: 902 ROM / 902 native instructions
- both fighters state4+bit14+bit16: 1071 ROM / 1071 native instructions

For all three cases:

- architectural snapshot: exact
- mutable memory: exact
- instructions: delta 0
- calls: delta 0
- returns: delta 0
- interrupt entries: delta 0
- interrupt returns: delta 0

Clean source candidate: `2580d192625588e0813c74ce03e58f26515d7d31`.
