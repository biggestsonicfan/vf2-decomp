# Post-frame texture bridge

> Superseded in v0.0.19 by `post_frame_bridge_v0019.md`; this file records the v0.0.18 inner-loop boundary.

## Proven interval

After the recovered first scheduler sweep returns to `0x0000a014`, the original
runtime performs texture preparation and frame-side work before the next
scheduler traversal. The complete observed bridge contains 1,270,822 original
i960 instructions.

v0.0.18 replaces four repeated semantic block kinds:

| Entry | Meaning | Live count |
|---:|---|---:|
| `0x0004c868` | byte-run expansion | 1,752 |
| `0x0004cce8` | 16-bit word-run expansion | 1,752 |
| `0x0004c928` | recursive texture-tree expansion | 4 |
| `0x0004ce88` | texture color conversion | 28 |

The blocks account for 712,821 instructions. The remaining 558,001
instructions continue through the bounded interpreter.

## Byte and word runs

The byte loop writes the low byte of `r8` `g1` times, decrementing `r10` by two
after each store. The word loop writes the low halfword of `g2` `g3` times,
incrementing `r10` by four. Both recover exact loop counters, destination
postconditions, equal condition code and instruction totals.

## Recursive tree expansion

Entry `0x0004c928` walks an observed binary texture tree to level eight. Leaf
records use table `0x0004ad78`; output slots are eight bytes and sibling stride
is `1 << level`. Recovery reproduces nested procedure-call/return counts,
maximum recursion depth and the caller return state. Only the proved node and
leaf encoding is accepted.

## Color conversion

Entry `0x0004ce88` converts the observed work-RAM source buffer into strided
texture destinations while updating the two nibble streams at `0x0055c2ef` and
`0x0055c2ee`. Recovery is intentionally restricted to:

```text
0x005500f4 == 0
0x00500000 == 0
0x0050008c == 0
```

Suspend, alternate frame-state and wait paths return unsupported rather than
being inferred.

## Differential proof

The reference machine executes the original code. The native machine invokes
recovered C at accepted entries. Instruction, procedure-call, procedure-return,
IP and frame-depth postconditions are checked per block. Mutable memory is
compared for every recursive expansion and color conversion, every 128th run
block, and completely at the second `fa_game_info` entry.

```text
recovered blocks:             3536
recovered instructions:     712821
interpreted instructions:   558001
memory checkpoints:             58
final CPU and memory:          MATCH
```
