# selector17 index6 BOOKKEEPING — VS diagram

The BOOKKEEPING handler is selected by `a4=0x86` through slot
`0x0005fed8 -> 0x0005c9b8`. States 8/9 form page `5/5`, titled
`VS GAME DATA 2` / `VS DIAGRAM`.

State 8 builds the static page and resets `a7=0`. State 9 maps the
visible fighter selector through the ROM table at `0x0005dca4`:

`0,1,2,3,4,5,6,7,8,10`

so the ten selectable fighters are AKIRA, JACKY, SARAH, KAGE, LAU,
JEFFRY, PAI, WOLF, SHUN and LION; hidden record 9 is skipped.

Each fighter owns a 48-byte VS record beginning at
`0x01d036a4 + 48 * mapped_fighter`. State 9 renders nine win/loss
pairs from byte offsets `0,4,...,32` and the final visible pair from
byte offset `40`, again skipping the hidden-character pair at 36.
Wins are drawn at tile column 23, losses at 31, on rows 14..32.

The rate bar starts at column 38 and is 20 tiles wide. ROM helper
`0x0005dc10` converts `wins/(wins+losses)` to 160 eighth-tile units,
emits full tile 7, one optional partial tile `7+remainder`, and pads
with spaces. A zero total renders `'-'` followed by spaces.

Exact full-frame measurements from `0x0000a6c0` to `0x0000a010`
with an all-zero VS record:

- idle: 1904 instructions, 34 calls, 35 returns
- fighter + (`0x1000`): 1908 / 34 / 35
- fighter - (`0x2000`, selector 0 wraps to 9): 1906 / 34 / 35
- service/page + (`0x08001008`): 1913 / 34 / 35; both `a7` and `a5`
  advance, with page 5 wrapping to construction state 0
- page - (`0x200`): 1907 / 34 / 35; state 9 goes to state 6
- TEST exit (`0x04000104`): 16173 / 50 / 51

Synthetic nonzero records were also measured to recover dynamic bar
accounting. These fixtures retained the canonical fighter-advance input
`0x1000`; changing only the first pair gave:

- 1/1: 1991 instructions, 44 calls
- 1/0: 2060 instructions, 54 calls
- 0/1: 1921 instructions, 34 calls
- 3/1: 2026 instructions, 49 calls
- 1/2: 1968 instructions, 41 calls
- 2/1: 2017 instructions, 48 calls

Subtracting the measured four-instruction fighter-advance path from
these fixtures establishes the state-9 bar delta: `13 + 7*T` for an
exact tile boundary, `11 + 7*T` when a partial tile is emitted, and one
less instruction for a completely full 20-tile bar. Each emitted
full/partial tile adds one nested `0x8440` call.
