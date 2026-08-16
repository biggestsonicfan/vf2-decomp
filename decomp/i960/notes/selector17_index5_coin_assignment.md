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

COIN CHUTE TYPE (`a5=1`) is a direct COMMON/INDIVIDUAL controller. Either edit
direction toggles bit 0 after clearing manual-mode bit 1, mirrors the complete
mode word to backup SRAM, and recomputes the 15-byte checksum. From the measured
COMMON default, both directions produce INDIVIDUAL and checksum `0xa417`.
The + path consumes 4,405 instructions/38 calls/39 returns and the - path
consumes 4,402 with the same call/return counts.

`CREDIT TO 1P START` (`a5=2`) uses an index at `0x3329`, and `CREDIT TO VS
START` (`a5=3`) uses an index at `0x332c`. Each index wraps over 0..14. Two ROM
lookup tables at `0x5bc74` and `0x5bc84` map the index to the visible START and
CONTINUE credit counts. The recovered C reads these ROM tables directly rather
than duplicating them, preserving the original representation where only valid
START/CONTINUE combinations are expressible.

For example, the measured default index 2 maps to 2 credits for START and 2 for
CONTINUE. Index 3 maps to 3/1 and index 1 maps to 2/1. The derived bytes are
stored at `0x332a/0x332b` for 1P and `0x332d/0x332e` for VS and mirrored to
backup SRAM together with the index. Both controllers have 4,190-instruction
idle frames. Their + and - edits consume 4,405 and 4,402 instructions with 38
calls; measured checksums are `0x7ec6`/`0x5fd7` for 1P and
`0x3e89`/`0x63d8` for VS.

`COIN/CREDIT SETTING` (`a5=4`) uses preset index `0x3324`, wrapping over 0..25.
The measured default is preset 0, displayed as setting #1; + selects preset 1
and - wraps to preset 25. Those transitions consume 4,403 and 4,401
instructions with 37 calls and yield checksums `0xd2a2` and `0x2511`.
The ROM contains additional transition tables at `0x61500`, `0x6151a`, and
`0x61534` for preserving valid preset relationships when the chute mode changes.

The recovered controller cut now covers idle/+/- for COIN CHUTE TYPE,
CREDIT TO 1P START, CREDIT TO VS START, and COIN/CREDIT SETTING from the
measured default configuration. All four share the same persistence/checksum
path; the two credit handlers also share the same ROM-driven START/CONTINUE
pair derivation. This intentionally models the coin system as one schema rather
than four unrelated handlers.

MANUAL SETTING is a nested state machine selected by `a7` rather than another
flat main-menu handler. Entering it sets `a7 = 0` and opens a five-item editor:
EXIT, COIN TO CREDIT, BONUS ADDER, COIN CHUTE #1 MULTIPLIER, and COIN CHUTE #2
MULTIPLIER. The four editable values live at `0x3325..0x3328`, each wrapping
over 0..8. An edit sets bit 1 of the mode word through `0x0005c7fc` and then
recomputes the same 15-byte coin checksum.

The manual values use a compact `displayed quantity - 1` encoding. COIN TO
CREDIT `0..8` means 1..9 coins are required for one credit. BONUS ADDER zero is
`NO BONUS ADDER`; values 1..8 mean 2..9 coins give one extra coin. Each chute
multiplier `0..8` means one physical coin counts as 1..9 logical coins for that
chute. `0x0005c828` renders these rules directly as text, for example
`2 COINS 1 CREDIT`, `2 COINS GIVE 1 EXTRA COIN`, or
`1 COIN COUNTS AS 2 COINS`.

The recovered manual controller covers `a7=1..4` idle/+/- using one shared
0..8 circular editor. Idle consumes 2,266 instructions/18 calls/19 returns.
A + edit consumes 2,475 instructions/20 calls/21 returns; - consumes 2,473 with
the same call/return counts. All edits mirror the selected byte, set manual-mode
bit 1 in both work RAM and backup SRAM, and recompute the same 15-byte checksum.
The measured default-to-1/default-to-8 checksums are `0x2877`/`0x0d79` for
COIN TO CREDIT, `0xac11`/`0x6ecd` for BONUS ADDER, `0x00e1`/`0x59e8` for chute
#1 multiplier, and `0xff53`/`0xd49f` for chute #2 multiplier. All twelve
idle/+/- cases match ROM snapshots exactly.

Manual SERVICE navigation uses the same deferred-cursor state machine seen in
GAME ASSIGNMENT: the current marker is erased, `a7` wraps over 0..4, and the new
marker is drawn on the following frame. Forward normally consumes 2,270
instructions and 17 calls (2,271 for wrap 4->0); backward normally consumes
2,267/17 (2,268 for wrap 0->4).

The nested EXIT (`a7=0`) accepts either edit direction, restores `a7=0xff`, and
clears the 62x40 manual-editor region beginning at tile row 4 while preserving
the parent COIN ASSIGNMENT instruction rows. It does not modify the coin schema
or checksum. The + path consumes 12,392 instructions/19 calls/20 returns; the
- path consumes 12,389 with the same call/return counts. This completes the
nested MANUAL SETTING state machine; main-menu entry/exit/navigation remain the
next index-5 cut.
