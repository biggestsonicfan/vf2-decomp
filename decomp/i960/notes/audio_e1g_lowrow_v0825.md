# E1g: 0x80 LOW row walk recovered -- the 0x80 class is complete (9/10 arc)

Status: LOW threshold-walk + head-leg conversion + three-round
LOW differential + two unit-leg conversions green in-file
(whole binaries, 0 FAIL). Full suite in the commit validation.

## Recovery (`dispatch_80`, past the head)

desc[2] < 0xf0: select = desc[2]&0x7f, table_base =
be32(0x6053aa + select*4), then the ROM walk (0x1144-0x1156):
d2 = $150f, stop at the first entry with d2 <= threshold
(unsigned bls), stride 6, no silicon cap. Then $1516 =
word(cursor+4), $1515 = byte(0x60c3e6 + ((base+byte(cursor+3))
& 0x7f)), falling into the shared 0x1174 scan (the HIGH/LOW
split now only picks the row; the scan block is common --
`row_byte` re-read from RAM like `row_word`, and the stale
"E1f-c door: refuse" comment is corrected to the inline sub).

Residue audit: 0x1138 andi.l LONG (d3 exact), 0x1162 andi.l
LONG (d2 exact -- move.b/add.b residue dies there), walk
cmp.b/add.b byte-only. The C walk is uncapped with per-read
OOB fail-closed: identical to silicon for every in-ROM walk,
refusing only where silicon would leave the audio ROM
(unmeasurable). Shadowing note: the shared scan's `row_word`
tripped C4456-as-error against the first draft's outer local
-- row state now lives in each arm, RAM is the handoff.

## Measurement (word-swapped table lesson)

First table dump read the raw file: the audio region is
LOAD16_WORD_SWAP, so every pointer/byte below was garbage
(sel3 -> 0x6000e655 fantasy). After swapping, sel3 =
0x6055e6, e0 = {th 0x52, add 0x13, word 0x008a}. The walk
math (e0 stop, $1516 = 0x008a) matched silicon first try;
only the $1515 curve was mispredicted (raw 0x80+i ramp vs
real 0xa7/0xb5 -- the print was read, not the bytes, the
E1b-fixture-rule class of error). C==S held throughout
(no RAM mismatch on any round); only the staged voice BYTE
was wrong (0x9f -> 0xa7), fixed, then all three rounds
matched first try.

## Differentials

Head leg (ring {0x81,0x06}, desc 0x80/0x01/0x03/0x10):
C OK + consume, $1516 = 0x008a, $1515 = 0xb5, S 0x265c in
295 steps, full-RAM (drain exclusions) + SCSP equality.
The old 0x10e2/31-step split pin is superseded (recorded,
not deleted).

LOW leg (0x80/0x05 packet, desc[2] = 0x03, $1516 = 0x008a,
$1515 = 0xa7, channel 0):

| round | shape | C post | S steps | HIGH equiv |
|---|---|---|---|---|
| 0 | teardown-normal | +2=0, 10/1f, 151e=ff | 531 | 534 |
| 1 | key+L1-arm | +2=0x41, 10/00 | 327 | 330 |
| 2 | miss-all | consume, v2 0xc0 kept | 306 | 309 |

Uniformly 3 under HIGH: the 1-iteration walk (cmp+bls) is 3
steps cheaper than the HIGH mulu/index path. Defer/release
need no LOW re-proof (chantable/+2-bit3 logic is
row-independent).

Unit legs (synthetic ROM): sel 1/15 stage a local 1-entry row
(th 0x7f, add 0, word 0x00ff); walk stops at e0, $1516 =
0x00ff, $1515 = 0x00, channel mismatch miss-all, OK +
consume. The 0x1132 door is gone -- no path refuses there.

## Consequence

The 0x80 class is now native end-to-end on both rows:
row filing (HIGH direct + LOW walk) -> scan -> 0x11d0 (4
arms) -> L1 countdown -> expiry -> key-off. Remaining 0x80
doors: none. The class's only unmeasured shape is walks with
d2 > 0x7f (skipping 0x7f terminators into following rows --
real tables bound production d2; explicitly not staged).
