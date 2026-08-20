# fa_game_info: exact state4 + bit14 native corridor (v0063)

## Scope

Recovered the isolated `fighter_state == 4` plus state-flag bit 14 corridor in `fa_game_info`, with bits 6/15/16 clear and the shared threshold non-negative.

## Recovery

The existing native `0x18144` bit-14 conditional body already matched the ROM semantically. Recovery required admitting the state4+bit14 dispatcher corridor into native `0x18644` and accounting for the extra state4 conditional rejoin.

Measured accounting relative to the prior state4 implementation:

- +1 instruction for each state4+bit14 fighter processed by `0x18144`;
- +1 fixed dispatcher instruction when the state4+bit14 corridor is selected.

## Validation

Real VF2 v2.2 ROM state, three controlled variants:

- fighter 0 state4+bit14 only: exact through `0x10dcc`;
- fighter 1 state4+bit14 only: exact through `0x10dcc`;
- both fighters state4+bit14: exact through `0x10dcc`.

For all three cases:

- official snapshot comparison: `Snapshots match.`
- instructions: delta 0
- calls: delta 0
- returns: delta 0
- interrupt entries: delta 0
- interrupt returns: delta 0

Clean source candidate: `e0b0d83671dafbe6887309e13ae291275057928b`.
