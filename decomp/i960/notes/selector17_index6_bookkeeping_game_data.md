# selector17 index6 BOOKKEEPING — hidden GAME DATA pair

The BOOKKEEPING jump table at `0x0005c9cc` continues beyond the
five user-facing pages. States 10/11 dispatch to `0x0005dce0` and
`0x0005e2bc`. State 10 builds the first per-character GAME DATA
screen (AKIRA), while state 11 renders its counters from the
512-byte block at `0x01d00000`.

State 10 clears a 62x44 area beginning at tile address `0x01000200`
and exposes the ROM strings GAME COUNT 1P/VS, TOTAL/AVG/MIN/MAX TIME,
CONTINUE/SET/DRAW counts, WIN BY K.O./RINGOUT/JUDGE counts, an 11-row
1P round table, and a 33-bin VS game-time histogram. It then enters
state 11.

State 11 layout recovered from `0x0005e2bc`:

- `+0x000/+0x004`: GAME COUNT 1P / VS
- `+0x008/+0x010`: 64-bit TOTAL TIME 1P / VS
- `+0x018/+0x138`: MIN TIME 1P / VS
- `+0x01c/+0x13c`: MAX TIME 1P / VS
- `+0x020/+0x140`: CONTINUE COUNT 1P / VS
- `+0x024/+0x144`: SET COUNT 1P / VS
- `+0x028/+0x148`: DRAW COUNT 1P / VS
- `+0x02c/+0x14c`: WIN BY K.O. 1P / VS
- `+0x030/+0x150`: WIN BY RINGOUT 1P / VS
- `+0x034/+0x154`: WIN BY JUDGE 1P / VS
- `+0x038`: eleven 16-byte 1P round records
- `+0x158`: thirty-three 32-bit VS histogram bins

Direct ROM measurements with a synthetic local-frame sentinel:

- state 10 body: 20564 instructions, 142 calls, 143 returns
- state 11 body with a zeroed 512-byte AKIRA block and valid SP:
  4545 instructions, 131 calls, 132 returns

The recovered frame accounting includes the already-measured
BOOKKEEPING dispatch/wrapper overhead: 20595/144/145 and
4576/133/134 respectively.

This cut admits both zero and non-zero GAME DATA for all sixteen hidden slots.

## All hidden character slots

The shared pair is reused for 16 slots, states 10..41. The body-name table is
`AKIRA, JACKY, SARAH, KAGE, LAU, JEFFRY, PAI, WOLF, SHUN, ACHO, LION, MUE,
KOJAC, -----, -----, -----`. The independent header table labels the same slots
`AKIRA, JACKY, SARAH, KAGE, LAU, JEFFRY, PAI, WOLF, SHUN, DURAL, LION, MUE,
KOJACKY, -------, -------, -------`; the ACHO/DURAL and KOJAC/KOJACKY aliases
are therefore preserved rather than normalized.

Direct ROM measurements of all 32 states with each 512-byte block zeroed show
that every odd render state is invariant at 4548/131/132 from the index-6
dispatcher, while construction-state cost is `20527 + 8*strlen(body_name)`
instructions with 142/143 calls/returns. Adding the outer frame overhead gives
the recovered accounting `20555 + 8*strlen(body_name)` / 144 / 145 for even
states and 4576 / 133 / 134 for odd states.

## Non-zero GAME DATA semantics

Controlled ROM probes on the AKIRA block establish the live formulas. Time
fields use 1/64-second ticks. TOTAL renders `(ticks>>6)` as D/H/M/S; AVG divides
ticks by GAME COUNT before the same shift and renders M/S; MIN/MAX are 32-bit
tick values rendered as M/S. Each 1P round record contains total, wins and a
64-bit tick accumulator; displayed seconds are `ticks>>6`, average is
`seconds/total`, and win rate is `1000*wins/total`. The 33 VS histogram words
are direct counters.

A non-zero GAME COUNT selects the numeric AVG helper, changing the measured
path by -27 instructions and +4 nested calls. A non-zero round total selects
the AVG/WINRATE path, changing that record by -24 instructions and +1 nested
call. Values that leave the small ROM decimal table retain the already-recovered
decimal helper's dynamic instruction delta. The bridge now renders non-zero
statistics for all sixteen slots rather than accepting only zeroed records.
