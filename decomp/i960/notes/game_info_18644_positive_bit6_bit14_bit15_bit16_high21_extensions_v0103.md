# `fa_game_info` positive state-8 bit-14/15/16 high-bit extensions

## Question

Does the existing positive state-8 corridor remain exact when bits 14, 15 and
16 are accompanied by high bit 21 and selected high bits from the already
measured high-bit family?

## Controlled measurements

The calibrated `0x164ac` boundary was reused with state 8, threshold `0`, and
the standard 12-case distribution: fighter 0 only, fighter 1 only and bilateral
flags, each with countdown `0/1` and mode bit 6 `0/1`.

| measured flag mask | high-bit composition | result |
| --- | --- | --- |
| `0x421c140` | `21+26` | 12/12 exact |
| `0x2021c140` | `21+29` | 12/12 exact |
| `0x4021c140` | `21+30` | 12/12 exact |
| `0x8021c140` | `21+31` | 12/12 exact |
| `0x2421c140` | `21+26+29` | 12/12 exact |
| `0xe421c140` | `21+26+29+30+31` | 12/12 exact |

All cases matched the reference snapshot and instruction/call/return counters.
The measurements exercised the existing common state-8 corridor; no new
semantic correction was required. The result does not generalize to other
unmeasured high-bit subsets.

## Reproduction

Use `validate_game_info_state4.py` with `--state 8`, `--threshold 0`, the
bit-14/15/16/21 plus selected high-bit `--extra-bit` arguments, `--mask 248`
for the pair cases, `--mask 504` for `21+26+29`, and `--mask 2040` for the
five-high-bit aggregate. Reuse `out/state8-positive.boundary.vf2snap` as the
`--base` snapshot when available.
