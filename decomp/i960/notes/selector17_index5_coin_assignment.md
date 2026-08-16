# Selector 17 bit-7 index 5: COIN ASSIGNMENT

Phase index 5 is entered naturally as `0x005000a4 = 0x85`, `a5 = 0`,
`a6 = 0xff`, and `a7 = 0xff`. The bit-7 table entry at `0x0005fed0` points to
`0x0005b558`.

The first observed frame is the COIN ASSIGNMENT screen and consumes 4,188 i960
instructions, 35 calls and 36 returns. Unlike GAME ASSIGNMENT, this first frame
does not initialize coin state: outside tile RAM it changes only the normal
`0x005ff602 = 0x56` scratch byte. The displayed values are a projection of the
coin configuration that was already loaded.

The main menu contains six high-level handlers in the table at `0x0005bc20`:
EXIT, COIN CHUTE TYPE, CREDIT TO 1P START, CREDIT TO VS START,
COIN/CREDIT SETTING, and MANUAL SETTING. The two CONTINUE costs displayed under
1P and VS are derived values, not separately selectable menu states.

Coin configuration is a separate persistent schema beginning at
`[0x0050016c] + 0x3320`. `0x0005ff54` computes the checksum over exactly 15
bytes (`0x3320..0x332e`) and stores the 16-bit result at backup SRAM
`0x01d03300`. This is distinct from the 29-byte GAME ASSIGNMENT block at
`0x3340` and its checksum at `0x01d03302`.

`0x3320` is a packed mode word. Bit 0 selects COMMON versus INDIVIDUAL coin
chutes. Bit 1 marks manual coin parameters; preset edits clear it, while edits
inside MANUAL SETTING set it before recomputing the 15-byte checksum.

`CREDIT TO 1P START` uses an index at `0x3329`, and `CREDIT TO VS START` uses
an index at `0x332c`. Each index wraps over 0..14. Two ROM lookup tables at
`0x5bc74` and `0x5bc84` map the index to the visible START and CONTINUE credit
counts. For example, the measured default index 2 maps to 2 credits for START
and 2 for CONTINUE; index 3 maps to 3/1; index 1 maps to 2/1. The derived bytes
are stored at `0x332a/0x332b` for 1P and `0x332d/0x332e` for VS.

`COIN/CREDIT SETTING` uses preset index `0x3324`, wrapping over 0..25. The ROM
contains transition tables at `0x61500`, `0x6151a`, and `0x61534` for preserving
valid preset relationships when the chute mode changes. The measured default
is preset 0, displayed as setting #1.

MANUAL SETTING is a nested state machine selected by `a7` rather than another
flat main-menu handler. Entering it sets `a7 = 0` and opens a five-item editor:
EXIT, COIN TO CREDIT, BONUS ADDER, COIN CHUTE #1 MULTIPLIER, and COIN CHUTE #2
MULTIPLIER. The four editable values live at `0x3325..0x3328`, each wrapping
over 0..8. An edit sets bit 1 of the mode word through `0x0005c7fc` and then
recomputes the same 15-byte coin checksum.

The first recovered C cut currently accepts the measured default entry state
only: COMMON chute type, preset #1, and 2/2 credit pairs for both 1P and VS. It
reproduces the ROM-observed tile writes and exact CPU post-state. Further coin
controllers are intentionally kept as separate ROM-backed cases until their
full preset/manual side effects are validated.
