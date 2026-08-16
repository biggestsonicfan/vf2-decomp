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

This cut intentionally admits only the measured zero-data state-11
path. The ROM reuses the same 10/11 bodies for later hidden character
slots; non-zero statistics and repeated slots will be generalized in
the next recovery cut.
