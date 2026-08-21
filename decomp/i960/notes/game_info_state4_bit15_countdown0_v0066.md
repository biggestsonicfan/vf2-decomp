# fa_game_info: exact state-4 + bit-15 countdown-zero corridor (v0066)

## Scope

Completed the remaining isolated state-4 + bit-15 dispatcher corridor when the global countdown byte at `0x0050a0b6` is zero.

The nonzero-countdown state-4 + bit-15 path was already native. This change removes the countdown-zero restriction while keeping mixed bit-15 combinations fail-closed.

## Validation

Using the calibrated `fa_game_info` entry snapshot and real VF2 v2.2 ROMs, three controlled countdown-zero fixtures were compared through the full chain to `0x10dcc`:

- fighter 0 state4 + bit15: exact, 720 instructions
- fighter 1 state4 + bit15: exact, 720 instructions
- both fighters state4 + bit15: exact, 731 instructions

For all three cases:

- architectural snapshot: exact
- mutable memory: exact
- instructions: delta 0
- calls: delta 0
- returns: delta 0
- interrupt entries: delta 0
- interrupt returns: delta 0

The measured accounting correction is one additional instruction per fighter in the state4 + bit15 corridor.

Clean source candidate: `28ace6f41b50fd4ed3bdce2b0348a2b569548b76`.
