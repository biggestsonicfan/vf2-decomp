# Selector 17 bit-7 index 3: display test 1/2

A controlled phase-17 forward+reset transition from phase 2 produces
`0x005000a4 = 0x83`, `0x005000a5 = 0`, and `0x005000a6 = 0xff`. The primary
bit-7 table entry at `0x0005fec0` points to `0x00059f34`; secondary state zero
selects `0x00059f88`, which calls the display-pattern renderer at `0x00060600`.

The first visit builds DISPLAY TEST 1/2 algorithmically. Reference execution
from `0x0000a6c0` to `0x0000a010` consumes 24,451 i960 instructions, 20
procedure calls and 21 procedure returns and advances the secondary selector to
`a5 = 8`.

The renderer is not stored as a captured tile dump. Its measured structure is:

- rows 4 through 39, columns 7 through 54, split into four 9-row bands;
- per-band map bases `0x9d80`, `0x9d00`, `0x9c00`, and `0x9c80`;
- sixteen intensity entries per row, each spanning three columns;
- four identical 4-bpp pattern banks beginning at `0x010b8020`, `0x010b9020`,
  `0x010ba020`, and `0x010bb020`;
- each bank contains fifteen solid 8x8 tiles whose sixteen 16-bit pattern words
  are `0x1111` through `0xffff` for intensity levels 1 through 15;
- four 16-entry palette ramps at `0x01800700`, `0x01800720`, `0x01800740`, and
  `0x01800760` for red, gray, green, and blue. Levels 0 through 14 use even
  5-bit component values 0,2,...,28 and the final level uses full scale 31.

The screen labels are reproduced at their measured coordinates: DISPLAY TEST
1/2, COLOR/BIAS/GAIN/SCROLL, RED/GREEN/BLUE/EXIT, and the lever/PUNCH/KICK
instructions. The observed stack scratch words are also reproduced rather than
ignored by the fast path.

Secondary state `a5 = 8` is the cursor-entry transition for EXIT. Its next
visit consumes 87 instructions, 4 calls and 5 returns, writes the cursor tile
`0x801c` at row 45 column 22, advances `a5` to 9 and otherwise leaves the test
pattern intact.

The selector states form pairs: even states 2/4/6/8 draw the cursor for
RED/GREEN/BLUE/EXIT and advance to active states 3/5/7/9. The active state 9
has been measured as the initial interactive loop; its idle path renders the
seven numeric fields from `[0x0050016c] + 0x3356..0x335c` (initially
64/64/64, 37/37/37 and scroll 31) without regenerating the pattern or palette.
Further active controls remain separate ROM-backed recovery cases.
