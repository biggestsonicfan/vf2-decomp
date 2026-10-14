# S3a+S3b: setup scan + HIGH authoring (v0825)

Status: 0x9ba teardown scan and 0xa8c HIGH authoring (incl. the
0x1014 picker) recovered in C, refusing at 0xf12 / 0xc1e with
packets held. Four drain-entry differentials C==S (S3a x2, S3b
x1, rolled-forward arm); RED pre-fix observed on all. Sound
gate green.

## S3a: 0x9ba scan (0x9ba-0xa3a -> 0xf12)

Teardown sweep over 32 voices: EVERY voice with +2 != 0, +3 ==
0xba, +4 == 0x0f (+2 = 0, d2 = +9 byte, +9 = 0, SCSP[slot] =
0x10, ram[0x2000+slot] = 0, $151e--, V1-shaped +9 rebalance with
the current d2). No key-off (stepper-pinned: no 0x1f write).
d2's long-residue never participates (move.b in, cmp.b read).
Legs (ring {0x9f,0x2a,0x01} -> $1515 0xb6 door):
- two (616 steps): v0{+9 3}/v1{+9 7} both torn; v0's
  rebalance takes v1 7->6 first; $151e 0x20->0x1e.
- skip (203 steps): +3/+4/+2 mismatches fully staged, $151e
  untouched.

## S3b: 0xa8c HIGH authoring (0xa8c-0xb72 -> 0xc1e door)

Entered with a1 = armed voice, a0 = drain descriptor (Loop-B
fall-through; the C threads both). +2 0x40->0x50->0x58,
slot key, +6 = $1516, +3 = $1515, +8 = $1510 mode, +4 =
channel, +9 = 0xff / +5 = $152e + bumps (ch != 0xf), volume
mulu/lsr/not ((voice8*desc10)>>8, ~, >>1 -> SCSP[slot+0x0d];
pure bytes, residue-free), 0x1014 picker (SCSP[slot+0x10]),
sample-slot/zero words, channel-table SCSP[slot+0x12],
$150d/$1511 SCSP bytes. Odd slot fails closed (word/long SCSP
stores are address-error shapes on ROM; e0-tail precedent).
Leg (294 steps): voice 58/80/03/04/0140/09, $152e 5->4->5,
SCSP[0x30] = 0x7000, d2 = 0x1004 -- C==S incl. the stepper's
live d2 against the report's lookup word, proving the
subroutine's discarded half too.

## Reachability finding (shapes S3c)

- The ch-0xf 0xaf0 path (+9 = $151e seeding) is
  DRAIN-UNREACHABLE: ch-f packets take Loop-B's 0x9a2 branch
  and never arm, while authoring only runs on the just-armed
  voice (whose +4 = this packet's non-f channel). Proof by
  construction of the dispatch chain, not by snapshot.
- Same for the 0xb76 LOW authoring branch (desc[2] < 0xf0
  routes to 0xa3e pre-Loop-B; nothing mutates desc[2] between
  the split and 0xaac). Both need synthetic-entry (F11-style)
  differentials in S3c; their bodies are unrecovered and the C
  refuses the LOW split at 0xa8c.
- The 0xc1e tail (sample-table -> SCSP, bra into the 0xc68
  movem playback engine) is S3c survey + S4; the engine's
  block-copy loops need the sample-ROM model first.

## Rolled forward

- loopb-arm leg now runs through authoring to 0xc1e (294
  steps; voice/SCSP depth lives in a8c-hi).
- setup9ba leg runs the (empty) scan to 0xf12 (201 steps).
",
