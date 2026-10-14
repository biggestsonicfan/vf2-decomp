# S2b: 90 + 80 primer heads, default-arm removal (v0825)

Status: `dispatch_90` head is payload + ring byte 2 (was command +
payload); `dispatch_80` head is payload with a byte-add base (was
command with a word-add); unlisted classes ignore via the 0x856
rts (the old default->90 mapping is retired, contradicting the
C's own switch-00 leg). Four new drain-entry differentials green
on first run; sound gate green (hardware + triangle + cdispatch).

## The primer proof (same trap class as S2a)

The switch's 0x90 beq lands ON 0x858 (`move.b (a3)+,d0`), not
past it -- the C's own switch-90 leg already pinned entry 0x858
in 4 steps, but the body was written as if entry were 0x85a with
d2 = command. With a3 at ring+1 (drain reality): head = payload
(ring byte 1), second = ring byte 2. The drain-entry legs prove
it: ring {0x93, 0x05, 0x02} gives base 0x15 (0x05 + desc[4]
0x10), never the old 0xa3 (0x93 + 0x10); mode is
ROM[0x60c6a6+2] = 0x11 on real ROM, never the old +5 file.

d0-upper cleanliness (no masking on the 90 mode path, none
needed): the drain's 0x7be/0x7c8 andi.l chain zeroes the upper
entry-independently and every move.b preserves the zero, so
d0.l == second exactly at the 0x872 table read. The d3 long-
residue past subi.b/andi.w is likewise exact: the 0x894 pick
reads d3.w only. Both documented in the C.

## 80 head (same evidence, one extra leg)

Switch-80 leg enters at 0x10d0 (primer included, 6 steps), so
d2 = payload here too. Base = (payload&0x7f) + desc[4]-byte:
0x10da disassembles as `add.b`, so desc[5] never participates
(the old word-add was print-era). Handler pin moves 0x10d2 ->
0x10d0. Leg: ring {0x81, 0x06, x} -> base 0x16, drain to the
0x10e2 split load in 31 steps, C==S.

## Legs (test_cdispatch.c, all real-ROM, all C==S + SCSP + events)

- drain-90-high: desc[2] = 0xf5, drain 0x7a6 -> 0x884, 36 steps.
- drain-90-low: desc[2] = 0x03, drain -> 0xa3e, 36 steps. The
  equal counts pin the split's symmetry; the TARGETS pin the
  direction (silicon side -- the C refuses at 0x878 and claims
  neither arm; S2c owns both).
- drain-90-trap: byte2 = 0, drain -> 0x10e2, 31 steps; base
  filed, mode untouched, both sides.
- drain-80: drain -> 0x10e2, 31 steps; base 0x16 both sides.
- F11 (S1) still proves the HIGH path's fall-through to 0x902.

## Default arm (v0819 divergence closed)

Unlisted classes: handler 0x856, channel filed, no $150f/$1510
writes, matched counted, scan continues (a later descriptor may
still hit a listed class), drain consumes (OK, count 0, cursor
4, handled). The three stale default legs (->90 body, base/mode
pins, trap shape) are rewritten as ignore pins; the v0792
"d3 falls through into the 90 head (bne.w)" comment was a +2-
base misread of bne.s -> 0x856 rts, retired.

## Fixture re-staging (intent-preserving)

90 trap: base 0xa0 -> 0x10, handler 0x858. 90 prefix: base 0xa3
-> 0x15, marker +5 -> +2 (value 0x44 kept). 90 mid leg: base
0x1f -> 0x85, marker out of the dead +0x10000 file to +2 (0x2a
kept). 80 legs: handler 0x10d0, bases 0x21 -> 0x00 and 0x0f ->
0x01 (payload-0 fixtures: the silicon legs carry the
discrimination). ch-0/A0/B0/C0/e0 legs untouched.

## Deliberately deferred

- 90 HIGH/LOW bodies + Loop-B door (S2c): the C refuses at the
  0x878 split with head filed, packet held.
- B0/C0 heads (same primer audit applies: entries 0x13ca/0x1e5a
  both land on their primers, so d2 = payload there too -- the
  B0 table index and C0 bset/file are command-derived today).
  Changing OK-returning consumed legs needs dedicated
  differentials (S2d backlog), not a drive-by in this slice.
- A0 refuses at entry (head irrelevant); e0 fixed in S2a.
",
