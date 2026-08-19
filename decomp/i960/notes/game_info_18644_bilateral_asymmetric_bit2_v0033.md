# `fa_game_info` `0x18644`: asymmetric state8/state8+bit2 (`0x100<->0x104`)

This note records ROM-backed differential validation of the exact asymmetric fighter-state compositions `0x100/0x104` and `0x104/0x100` in the recovered `0x00018644` child of `fa_game_info`.

## State definition

The state value is read from each fighter record at `fighter + 0x1a4`.

`0x100` is bit 8. `0x104` is bit 8 plus bit 2 (`0x004`). Both orientations were patched at the real fighter-state source before entering the caller boundary at `0x000164ac`.

## Pure-ROM reference

Both orientations produced the same caller-to-task instruction counts:

| orientation | countdown | mode bit 6 | instructions | final instructions | calls | returns |
|---|---:|---:|---:|---:|---:|---:|
| `0x100 -> 0x104` | 0 | 0 | 210 | 14275966 | 10249 | 10248 |
| `0x100 -> 0x104` | 0 | 1 | 215 | 14275971 | 10249 | 10248 |
| `0x100 -> 0x104` | 1 | 0 | 186 | 14275942 | 10249 | 10248 |
| `0x100 -> 0x104` | 1 | 1 | 191 | 14275947 | 10249 | 10248 |
| `0x104 -> 0x100` | 0 | 0 | 210 | 14275966 | 10249 | 10248 |
| `0x104 -> 0x100` | 0 | 1 | 215 | 14275971 | 10249 | 10248 |
| `0x104 -> 0x100` | 1 | 0 | 186 | 14275942 | 10249 | 10248 |
| `0x104 -> 0x100` | 1 | 1 | 191 | 14275947 | 10249 | 10248 |

## Native recovery

Only the exact asymmetric `0x100<->0x104` compositions were admitted in the existing bilateral-state and threshold guards. Unmeasured mixed states remain fail-closed.

Before accounting adjustment, isolated native execution matched the ROM architectural state and memory in all 16 probes (two orientations x two call sites x countdown x mode bit 6), with procedure calls and returns already exact.

The instruction-accounting corrections are the same measured corrections used by the validated `0x104/0x104` corridor:

- return `0x000164b0`: `-2` without countdown and no correction with countdown;
- return `0x000164c4`: `-6` without countdown/mode-bit-6 clear, `-5` without countdown/mode-bit-6 set, and `-1` with countdown.

## Chained proof

Final acceptance executed:

`caller -> native 0x18644 #1 -> caller ROM -> native 0x18644 #2 -> tail ROM -> 0x10dcc`

for both orientations and all four countdown/mode-bit-6 combinations.

All eight cases matched the pure ROM exactly in CPU architectural state, mutable memory, executed instructions, procedure calls and procedure returns.

Validated recovery commit: `ec67a209d8343e14d88936358324db2e6b8b1a83`.
