# Selector 17 bit-7 index 4: GAME ASSIGNMENT

Phase index 4 is entered with `0x005000a4 = 0x84`, `a5 = 0`, and `a6 = 0xff`.
The primary bit-7 table entry at `0x0005fec8` points to `0x0005a680`.

The menu is driven by sixteen `{handler, descriptor}` pairs beginning at
`0x0005b340`. Descriptor fields are: label destination at `+0`, value
destination at `+4`, payload offset at `+8`, renderer/selection flags at `+0xc`,
string-table pointer at `+0x10`, and the inline label text at `+0x14`.

`0x00061260` renders all labels. `0x00061288` renders values generically:
low flag bits 0..1 select byte/halfword/word reads from `[0x0050016c] + payload`;
bit 2 extracts one packed bit, with the bit number encoded in flags; bit 8 maps
the value through the descriptor string table; bit 10 suppresses value output.
Bit 9 marks entries skipped by SERVICE navigation.

Observed entries are EXIT, MATCH COUNT(1P), MATCH COUNT(VS), DIFFICULTY,
ENERGY MAX(1P), ENERGY MAX(VS), STAGE WIDTH, ADVERTISE SOUND, CONTINUE, DRINK,
COUNTRY, DISPLAY TYPE, VS FINISH, RANKING MODE, VERSION, and INITIALIZE.
ENERGY MAX and STAGE WIDTH entries carry the skip flag and are not selectable.

The recovered C keeps this ROM-driven layout rather than hardcoding the visible
menu strings. It also copies the two common instruction records referenced by
`0x0005ff14` and `0x0005ff18`, preserving unwritten tile gaps exactly as the ROM
does.

Selection state zero is EXIT. On a stable frame, idle EXIT consumes 3,044 i960
instructions, 38 calls and 39 returns. SERVICE forward (`0x00500704 = 0x1000`)
moves `a5 0 -> 1` in 3,050 instructions, 37 calls and 38 returns; SERVICE back
(`0x2000`) wraps `a5 0 -> 15` in 3,048 instructions, 37 calls and 38 returns.
Navigation erases the old cursor and the newly selected cursor is drawn on the
following frame, matching the ROM state machine.

MATCH COUNT(1P), state `a5 = 1`, is the first recovered editable assignment.
Its idle frame consumes 3,045 instructions, 38 calls and 39 returns. The
`0x00060b84` edit delta uses canonical `0x00500704 = 0x100` for +1 and `0x200`
for -1. The value at `[0x0050016c] + 0x3340` wraps over the inclusive range
2..5: the measured default 2 becomes 3 on +1 and wraps to 5 on -1.

An edit is persisted byte-for-byte to backup SRAM `0x01d03340`, then the same
29-byte assignment/configuration block beginning at `base + 0x3340` is passed
to the existing CRC routine and the resulting 16-bit CRC is stored at
`0x01d03302`. The measured 2->3 transition consumes 3,417 instructions, 40
calls and 41 returns and yields CRC `0x785c`; the 2->5 wrap consumes 3,415
instructions with the same call/return counts and yields CRC `0x7a4a`. The
value is rendered before the edit takes effect, matching the ROM's handler
ordering.

MATCH COUNT(VS), state `a5 = 2`, uses the same circular 2..5 controller and
persistence path against `base + 0x3341`. Its instruction/call counts are
identical to the 1P handler. From the measured default 2, +1 produces 3 and CRC
`0xac2e`; -1 wraps to 5 and produces CRC `0x7756`. The recovery therefore
shares one parametrized MATCH COUNT implementation across the two descriptors.

DIFFICULTY, state `a5 = 3`, wraps over 0..3 and maps through the ROM string table
EASY/NORMAL/HARD/HARDEST. The measured default is 1 (NORMAL). +1 produces HARD
in 3,430 instructions, 41 calls and 42 returns with CRC `0x2dfd`; -1 produces
EASY in 3,427 instructions with the same call/return counts and CRC `0x5a6a`.

Difficulty edits call `0x0005b2cc`, which derives the read-only assignment
fields from three four-entry ROM tables rather than arithmetic in the handler.
For EASY/NORMAL/HARD/HARDEST respectively, ENERGY MAX(1P) is
176/160/144/128, ENERGY MAX(VS) is 220/200/180/160, and the STAGE WIDTH index
is 16/15/14/13. The recovered C reads these tables at `0x5b32c`, `0x5b334`, and
`0x5b33c`, updates both work configuration and backup SRAM, and then recomputes
the same 29-byte configuration CRC.
