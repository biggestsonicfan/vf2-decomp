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
