# `fa_game_info` `0x18644`: bilateral state8+bit2 (`0x104/0x104`)

This note records ROM-backed differential validation of the exact bilateral fighter-state composition `0x104/0x104` in the recovered `0x00018644` child of `fa_game_info`.

## State definition

The state value is read from each fighter record at `fighter + 0x1a4`.

`0x104` is bit 8 (`0x100`) plus bit 2 (`0x004`). Both fighters were patched at that real state source before entering the caller boundary at `0x000164ac`.

## Reference matrix

Pure-ROM execution from `0x000164ac` through scheduler return `0x00010dcc` produced:

| countdown | mode bit 6 | caller-to-task instructions | final instructions | calls | returns |
|---:|---:|---:|---:|---:|---:|
| 0 | 0 | 210 | 14275966 | 10249 | 10248 |
| 0 | 1 | 215 | 14275971 | 10249 | 10248 |
| 1 | 0 | 186 | 14275942 | 10249 | 10248 |
| 1 | 1 | 191 | 14275947 | 10249 | 10248 |

## Native recovery

The recovery admits only the exact `0x104/0x104` bilateral composition in the existing bilateral-state and threshold guards. Other unmeasured mixed states remain fail-closed.

The measured instruction-accounting corrections are call-site specific:

- return `0x000164b0`: `-2` without countdown and no correction with countdown;
- return `0x000164c4`: `-6` without countdown/mode-bit-6 clear, `-5` without countdown/mode-bit-6 set, and `-1` with countdown.

## Chained proof

Final acceptance executed:

`caller -> native 0x18644 #1 -> caller ROM -> native 0x18644 #2 -> tail ROM -> 0x10dcc`

for all four countdown/mode-bit-6 combinations.

All four cases matched the pure ROM exactly in CPU architectural state, mutable memory, executed instructions, procedure calls and procedure returns.

Validated recovery commit: `623b6660e802a17bfcb19d0cf77707a89e459417`.
