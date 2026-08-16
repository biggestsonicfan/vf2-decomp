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
instructions. The ROM does not write every visually blank separator as an
attribute-space tile. Strict comparison exposed eleven bytes where a combined
label write had produced `0x8020` while the reference preserved an existing
`0x0020`; the final recovery therefore emits COLOR, BIAS and GAIN/SCROLL as
separate measured runs. The observed stack scratch words are also reproduced
rather than ignored by the fast path.

The cursor states form pairs. Even secondary states 2/4/6/8 draw the cursor for
RED/GREEN/BLUE/EXIT and advance to active states 3/5/7/9. Each measured cursor
transition consumes 87 instructions, 4 calls and 5 returns. The implementation
uses one parametrized cursor path rather than four duplicated state bodies.

The active EXIT state (`a5 = 9`) has an idle path of 271 instructions, 11 calls
and 12 returns. It renders the seven numeric fields from `[0x0050016c] +
0x3356..0x335c` (initially 64/64/64, 37/37/37 and scroll 31) without
regenerating the pattern or palette. Measured navigation from EXIT is:

- `0x00500704 = 0x1000`: `a5 9 -> 2` (RED), 47 instructions;
- `0x00500704 = 0x2000`: `a5 9 -> 6` (BLUE), 48 instructions;
- `0x10`, `0x100`, or `0x200`: `a5 9 -> 10`, consuming 52, 53, or 54
  instructions respectively and entering DISPLAY TEST 2/2.

DISPLAY TEST 2/2 (`a5 = 10`) is another algorithmic renderer, not a stored tile
dump. It writes a 64x48 border using the measured edge/corner tiles
`0x8018/0x8011/0x8019`, `0x8013/0x800f/0x8012`, and
`0x801a/0x8010/0x801b`, then overlays `DISPLAY TEST 2/2` and
`PUSH TEST BUTTON TO EXIT`. Interior rows preserve columns 62 and 63 as
attribute spaces (`0x8020`) rather than the body tile. The renderer also fills
the auxiliary tile block `0x0100d000..0x0100dbff` with `0xff`. The transition
consumes 13,927 instructions, 6 calls and 7 returns and advances to `a5 = 11`.

State 11 idles in 40 instructions, 2 calls and 3 returns. Measured TEST aliases
`0x4`, `0x10`, `0x100`, and `0x04000000` in `0x00500704` all take the shared
diagnostic teardown; the `0x10` path is two instructions longer. Teardown
clears bit 7 (`0x005000a4: 0x83 -> 0x03`), zeroes the auxiliary
`0x0100d000..0x0100dbff` block, restores the common diagnostic menu and
consumes 14,582 instructions (14,584 for `0x10`), 18 calls and 19 returns.

The active RED/GREEN/BLUE handlers are recovered as one channel-parametrized
path. Relative to baseline `0x00500700 = 0x0ff7f700`, toggling input bit 8
increments the selected channel bias, bit 9 decrements bias, bit 16 increments
gain and bit 17 decrements gain. RED touches offsets `0x3356/0x3359`, GREEN
`0x3357/0x335a`, and BLUE `0x3358/0x335b`. Bias is clamped to 0..255 and gain
to 16..255, matching `0x0005a310`. The handler also forces scroll to 31,
updates the persistent mirror at `0x01d03356..35c`, recomputes the 29-byte
configuration CRC stored at `0x01d03302`, and preserves the ROM ordering in
which the displayed numeric values are rendered before the adjustment takes
effect.

RGB navigation is likewise recovered from the measured state machine:
RED forward/back goes to GREEN/EXIT, GREEN goes to BLUE/RED, and BLUE goes to
EXIT/GREEN. The handlers retain strict input whitelisting: only the baseline,
one measured raw-input bit toggle, the two measured navigation directions, and
the measured DISPLAY TEST 2 transition are accepted.

The ROM transfer-table generator at `0x00002b4c` is implemented semantically.
For each level 0..31 it computes `sample = level * scroll`. A zero sample maps
to zero; otherwise each RGB channel computes `bias + ((gain * sample) >> 8)`
and saturates values at or above 256 to `0xffff`. The resulting value is
replicated across 64 entries, with each level advancing the destination by
`0x180` bytes. The channel tables live at palette RAM offsets `+0x10000`,
`+0x14000`, and `+0x18000`. No captured transfer-table dump is used.

The recovered numeric loop calls the existing decimal renderer through an
explicit forward declaration; this keeps the helper single-sourced while
preserving strict C17 builds with warnings-as-errors.
