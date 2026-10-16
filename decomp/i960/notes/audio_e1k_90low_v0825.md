# E1k: 90 LOW row walk recovered to the 0xf12 door (9/10 arc)

Status: LOW walk in `dispatch_90` + `test_dispatch_90_low`
differential + drain-90 head-leg repin green in-file (whole
binary, 0 FAIL). Full suite in the commit validation.

## Recovery

desc[2] < 0xf0: select = desc[2] & 0x7f, table_base =
be32(0x6053aa + select*4) -- the SAME table family as the
80-LOW walk (E1g) -- then the 0xa44 threshold walk (stride 6,
uncapped, per-read OOB fail-closed), filing $1516 (word
cursor+4), $1518 (byte cursor+2), $1515 (byte 0x60c3e6 +
((base + byte(cursor+3)) & 0x7f) with byte-add wrap), $1511
(desc[6]), then refuse at the 0xf12 voice allocator (S2/S3
setup territory -- not entered). Names follow the 90-HIGH
file convention (LOOKUP_CONTROL/META/WORD); $1515 stays raw
0x2515 per the file-local E1f precedent.

Residue audit (the slice's load-bearing bit): 0xa44 andi.w
and 0xa72 andi.w are WORD ops, so d3/d4 uppers SURVIVE --
unlike E1g's LONG pair -- yet every use reads .w/.b only
(0xa4a move.l (a1,d3.w), 0xa52 cmp.b, 0xa6e add.b, 0xa7c
move.b (a1,d4.w)). Same determinism conclusion as E1g by a
different route (v0821 byte-domain rule); the worked bit
example is in the code comment (0xBE1F & 0x007F = 0x001F).

## Differential (a8c-hi-shaped, LOW desc)

Ring {0x93, 0x05, 0x01}, desc 0x80/0x03/0x05/[6]=0x07: base
0x05, sel 5 stops at e0 (0x05 <= 0x7f) with the wrapping
addend (0x05 + 0xfa = 0xff -> index 0x7f):

- C: UNSUPPORTED @0xf12, $1516 = 0x0090, $1518 = 0x00,
  $1515 = 0x27, $1511 = 0x07, packet held.
- S: 0xf12 arrival in 51 steps (park unstepped, dead-loop
  precedent -- the allocator past the door is unrecovered),
  identical cells, full-RAM (drain exclusions) + SCSP
  equality. First-try match on all four cells.

## Ripple (one leg)

test_drain_90_head LOW (desc[2] = 0x03, S stops at the 0xa3e
split, 36 steps, untouched): C pin moves 0x858 -> 0xf12
(the walk now runs; S-side pre-walk cells correctly
uncompared, $1511 == 0 both sides preserved via desc[6] =
0). Both arms park at 0xf12 through different doors.

## 90-class status

HIGH (row files -> Loop-B early door, S2d) + LOW (walk
files -> allocator door, this slice) both native to their
doors. Past the doors: Loop-B/allocator/setup-authoring
(0x9ba/0xa8c volume math) stay S2/S3 territory. The 0x80
class (E1f/E1g) completes; the 0x90 class doors (cb2/d92
dead loops excepted) do not -- recorded, not blurred.
