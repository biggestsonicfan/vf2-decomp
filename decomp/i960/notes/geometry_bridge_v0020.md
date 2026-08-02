# v0.0.20 geometry-bridge evidence

## Validated totals

```text
bridge total:                 1,270,822 instructions
recovered C:                  1,268,004 instructions
interpreted remainder:            2,818 instructions
recovered blocks:                   135
memory checkpoints:                 135
recovered calls/returns:         229/272
```

Reference and recovered machines begin from the same first-task fixture and
then execute independently. Each accepted block is followed by CPU-counter and
mutable-memory comparison. The complete machines match again at the second
`fa_game_info` entry.

## New accepted helpers

| Entry | Count | Meaning |
|---:|---:|---|
| `0x0004d16c` | 4 | texture address-table construction |
| `0x00007fc0` | 9 | diagnostic text copy to tile RAM |
| `0x0004f944` | 1 | 48-glyph expansion |
| `0x00002de4` | 1 | observed 28-page palette upload |
| `0x0004cdb0` | 32 | texture-conversion loop controller |
| `0x0004cdd4` | 28 | conversion continuation |
| `0x00000b6c` | 8 | timer/wait update |
| `0x00002ec4` | 1 | video-status latch |
| `0x00002edc` | 1 | geometry frame commit |
| `0x00002f5c` | 1 | geometry command setup |
| `0x0000a154` | 1 | frame scratch clear |

## Geometry register sequence

At entry `0x00002edc`, accepted state has `g10 = 0x00800000`.
Recovered C performs the observed sequence:

1. clear geometry offset `0x00f0`;
2. write the previous command pointer from `0x00501004` to `0x00803008`;
3. read the board pointer at `0x00802008` and update the maximum distance in
   `0x00501008` when required;
4. advance the ring index at `0x0050100c` modulo four;
5. read the next pointer from the ROM table at `0x00007a00`;
6. store it in `0x00501004` and geometry register `0x00801008`.

The setup helper at `0x00002f5c` constructs an observed command word from the
signed 16-bit value at `0x005010de` and command class at `0x005010dc`, stores it
at `0x005010e0`, and writes it to the caller-selected geometry offset.

These names describe proven register behavior. They are not yet claims about
the full TGP FIFO or polygon-command format.

## Remaining boundary

The 2,818 interpreted instructions are concentrated in scheduler re-entry,
diagnostic thunks and geometry-preparation helpers rooted around
`0x00001f5c`, `0x000026ec`, `0x0000281c` and `0x00010d54`. They remain outside
the accepted C boundary.
