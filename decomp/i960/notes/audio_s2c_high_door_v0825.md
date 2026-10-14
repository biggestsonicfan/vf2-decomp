# S2c: 90 HIGH path to the Loop-B door (v0825)

Status: HIGH table path recovered in C (gate / $1511 select /
$1512/$1516 / $150d triple-branch / desc[13] clear), refusing at
the 0x902 Loop-B door with the packet held. Three drain-entry
differentials + one gate-hit differential, all C==S first GREEN
after the fix (RED pre-fix run observed); three synthetic-ROM
unit legs pin the exact table arithmetic. Sound gate green.

## Path (0x884-0x8fe, all disassembled)

- select = desc[2]-0xf0 (byte subi), &0x7f; a2 =
  be32(0x6032c6 + select*4) + (payload&0x7f)*6. The &0x7f is the
  whole index model (0x894 reads d3.w only -- long-residue note
  from S2b).
- Gate 0x8a6: `cmpi.w #$ff,(a2)` -- the empty word is 0x00ff
  (not 0xffff): hit -> 0x8b0 rts, head effects only, back to the
  drain scan (C: continue, consume at end).
- 0x8b2: d1 = byte(a2+4), or desc[6] when the table byte is the
  0x80 sentinel; $1511 = d1. $1512 = word(a2+2), $1516 =
  word(a2). Odd entry addresses fail closed (ROM move.w would
  address-error; the C refuses instead of tearing).
- 0x8d2 (self-corrected mid-slice): d1 = desc[13]; NONZERO files
  itself and clears desc[13] via bra 0x8f6 -- the desc[12]
  logic is SKIPPED. Only desc[13] == 0 reaches desc[12]: kept
  unless (d1&7) == 0, when d1 = byte(a2+5). $150d = d1. The
  first draft had this backwards (nonzero -> desc[12] path);
  the table5 silicon run showed $150d == 0x07 == ex-desc[13].
- Door: 0x8fa/0x8fe compute d4 = $150f&0x7f for Loop-B; $1515
  is filed by Loop-B's own first block (F11: still 0 at the
  door). The C refuses at 0x902 with $1515 == 0 pinned.

## Legs (test_cdispatch.c, real ROM, C==S + SCSP + events)

- drain-90h-keep (desc12=1, desc13=0): 11=00 12=e000 16=0140
  0d=01, drain -> 0x902 in 62 steps. (Agrees with F11's
  $2511/$250d pins from the head-entry side.)
- drain-90h-table5 (desc12=8, desc13=0): 0d=00=byte(a2+5),
  63 steps -- the count (not the value) proves the extra fetch.
- drain-90h-clear (desc13=7): 0d=07, desc13 cleared, 60 steps.
- drain-90g (payload 0x6a, table word 0x00ff): C OK consumed
  (handler 0x858, head 6a/09 only); silicon drain -> 0x265c in
  132 steps. Gate-hit payloads found by a one-off head-entry
  sweep (0x6a-0x70+ hit the 0x8b0 rts); sweep deleted, 0x6a
  hard-pinned.
- Pre-fix RED: C refused at the 0x878 split ($1511+ zero)
  while silicon filed them -- the four fail lines observed,
  then fixed.

## Unit legs (test_sound_board.c, synthetic ROM)

HIGH door exact arithmetic (pointer 0x605000, entry 0x60500c):
$1511=0x41/$1512=0x5678/$1516=0x1234/$150d=0x9a; sentinel
variant ($1511=desc[6], $150d=ex-desc[13], desc[13] cleared);
gate-hit variant (OK consumed, head only).

## Deferred

- 90 LOW path (0xa3e: row-table walk to the F12 bra) -- still
  refuses at the split (S2d with the B0/C0 heads, which share
  the primer audit: entries 0x13ca/0x1e5a land on their primers,
  d2 = payload there too).
- Loop-B proper (0x902+: $1515 file, $150e/$152e gates, scan,
  0x95a arm, exits to 0xa8c vs 0xf12) -- S2d.
",
