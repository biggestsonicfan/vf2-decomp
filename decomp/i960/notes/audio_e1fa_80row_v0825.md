# E1f-a: 0x80 HIGH row filing to the 0x1174 door (9/10 arc)

Status: `dispatch_80` HIGH arm + drain-entry differential green
in-file (whole binary, 0 FAIL). Full suite in the commit
validation.

## The L1-trigger discovery (why this arc exists)

No C-recovered path sets voice+2 bit0 (the L1 scan's service
bit: `|=0x10/0x08`, `=0x40`, `=0` only), so `maintain_voices`
is vacuously a no-op on every native shape -- sustained-voice
behavior had no trigger population. Full-image disassembly
sweep for `$0002(aN)` writers (v0807 method) found it:
`0x11f8: bset #0,$0002(a1)` inside the 0x80-class voice-scan
subroutine 0x11d0. The 0x80 class is the voice-lifecycle
driver: row filing ($1516/$1515) -> 32-voice scan (match
+3==$1515, +4==channel, +6==$1516, bit4/1 clear, low7
nonzero) -> per-voice jsr 0x11d0, whose +0xb-nonzero exit
keys the slot (SCSP[slot]=0x10) AND arms L1 (bset #0). The
+0xb-zero exit keys off and tears down (normal: +2=0, +9
snap, $151e--, rebalance; release: +5 path, $152e--).

## Arc map

- E1f-a (THIS): head split + HIGH row filing to 0x1174.
- E1f-b: voice scan 0x1174-0x11ce.
- E1f-c: 0x11d0 subroutine (3 exits).
- E1f-d: L1 countdown -> expiry -> key-off closed
  (`maintain_voices` proven; needs a bset-#0'd voice).

## E1f-a measurement

Synthetic drain fixture (emit/desc-poke precedent): packet
{0x80, 0x05}, desc[1]=0 (channel), desc[2]=0xf3 (HIGH,
select 3), desc[4]=0x07. S drain 0x7a6 -> 0x1174 in 49
steps: d0=0x1e ((5)*6), d2=0x0c (base&0x7f, andi.l-exact),
d3=0x0c (select*4), a1=0x60c3e6, a2=0x6038a6,
$150f=0x0c, $1516=0x00ff, $1515=0x90. Residue audit: 0x10f2/
0x1106/0x111c andi.l are all LONG, so d3/d0/d2 are exact
despite byte moves (unlike the 90 arm's andi.w long-residue).

Notable: $1516 = 0x00ff (empty row) is filed with NO gate
check -- unlike the 90 arm's 0x00ff rts. The scan match
(+6==0x00ff) is E1f-b's problem.

## Recovery

`dispatch_80` past the existing head: desc[2] < 0xf0 takes
the LOW door (0x1132, unrecovered -- data-dependent walk);
HIGH mirrors `dispatch_90_high` (be32 table pick, word row,
0x60c3e6 byte). Odd-entry fails closed (silicon address
error); ROM bounds via the table helpers. Forward
declarations added (helpers live below). Refuses at 0x1174.

## Ripple (one leg)

`test_drain_80_head` (desc[2]=0x03, LOW): C pin moves
0x10d0 -> 0x1132; S side (stop 0x10e2, 31 steps) untouched.
Stale "refuses at the split" comment fixed.
