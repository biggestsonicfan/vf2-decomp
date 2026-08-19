# `fa_game_info` `0x18644`: bilateral state8+bit2+bit4 (`0x114/0x114`)

This note records ROM-backed differential validation of the exact bilateral fighter-state composition `0x114/0x114` in the recovered `0x00018644` child of `fa_game_info`.

## State definition

The state value is read from each fighter record at `fighter + 0x1a4`.

`0x114` is:

- bit 8 (`0x100`)
- bit 2 (`0x004`)
- bit 4 (`0x010`)

It must not be confused with state8+bit1+bit4, whose exact value is `0x112`.

Both fighters were patched to `0x00000114` at their real state source before entering the caller boundary at `0x000164ac`.

## Reference matrix

Pure-ROM execution from `0x000164ac` through the scheduler return at `0x00010dcc` produced:

| countdown | mode bit 6 | instructions | calls | returns |
|---:|---:|---:|---:|---:|
| 0 | 0 | 202 | 10249 | 10248 |
| 0 | 1 | 207 | 10249 | 10248 |
| 1 | 0 | 202 | 10249 | 10248 |
| 1 | 1 | 207 | 10249 | 10248 |

The countdown flag does not change the total caller-to-task instruction count for this exact state; mode bit 6 adds five instructions.

## Native recovery

The recovery admits only the exact `0x114/0x114` bilateral composition in the existing state guards and threshold boundary. Other unmeasured mixed states remain fail-closed.

The measured instruction-accounting corrections are call-site specific:

- return `0x000164b0`: `-6` without countdown, `+8` with countdown;
- return `0x000164c4`: `-10` without countdown and mode bit 6 clear, `-9` without countdown and mode bit 6 set, `+7` with countdown.

## Chained proof

The final acceptance test executed the full sequence:

`caller -> native 0x18644 #1 -> caller ROM -> native 0x18644 #2 -> tail ROM -> 0x10dcc`

for all four countdown/mode-bit-6 combinations.

All four cases matched the pure ROM exactly in:

- CPU architectural state;
- mutable memory regions;
- executed-instruction count;
- procedure-call count;
- procedure-return count.

Final totals were `14275958` instructions with mode bit 6 clear and `14275963` with mode bit 6 set, always `10249` calls and `10248` returns.

Validated recovery commit: `c71e2b862956365b966097ea2b6fa8e96b70461b`.
