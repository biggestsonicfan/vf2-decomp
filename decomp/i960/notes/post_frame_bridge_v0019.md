# v0.0.19 post-frame bridge evidence

> Superseded for the geometry-register sequence by `geometry_bridge_v0020.md`.

## Accepted recovered blocks

| Entry | Count | Meaning |
|---:|---:|---|
| `0x0004c3f0` | 4 | symbol-table construction |
| `0x0004c4d4` | 4 | pair-table construction |
| `0x0004c6e0` | 4 | complete byte texture decoder |
| `0x0004c928` | 4 | recursive texture-tree expansion |
| `0x0004cc28` | 4 | complete word texture decoder |
| `0x0004ce88` | 28 | observed color conversion |

The accepted blocks account for 1,262,476 of the 1,270,822 instructions between
first-sweep completion and the second `fa_game_info` entry. Each block has an
explicit architectural post-state and is followed by a complete mutable-memory
comparison against an independent original execution.

## Frame synchronization

The observed wait loop is represented by a host event state machine. Four
visits to `0x00000f7c` or `0x00010f98` raise interrupt bit 0 and enter vector 12.
The state machine models the validation environment's frame event; it is not a
claim that four busy-loop iterations equal real Model 2 wall-clock timing.

## Geometry boundary

The first geometry mutation occurs at instruction `0x00002eec`, which executes
`st r6, 0x3008(g10)`. In the accepted state `g10 = 0x00800000`, giving target
`0x00803008`. The first changed byte is `0x00803009` because the low byte of the
stored value and the previous byte were both zero.

At this milestone no command semantics were assigned. v0.0.20 subsequently
recovered the observed command-ring register behavior; the full TGP and
renderer protocol remains unclaimed.
