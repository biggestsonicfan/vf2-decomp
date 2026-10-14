# S2d: Loop-B proper, three doors (v0825)

Status: Loop-B 0x902 recovered in C ($1515 file, $150e/$152e
gates, 32-voice scan, 0x95a arm) with all three exits as pinned
doors (0xa8c / 0x9ba / 0xf12, packets held). Four drain-entry
differentials C==S (doors + steps + full voice/SCSP state);
RED pre-fix observed on all four. Sound gate green.

## Body (0x8fa-0x99e + 0x9a2-0x9b6, disassembled)

- d4 = $150f&0x7f (d4.w-indexed pick; andi.w long-residue above
  bit 15 unread, same pattern as the S2b index notes); $1515 =
  ROM[0x60c3e6+d4]. Real ROM[0x60c3e6] = 0x80.
- $150e&0xf == 0xf -> 0x9a2 setup-gate: $1515 0xb6/0xb8 doors
  0x9ba, else 0xf12. (Payload 0x2a -> base 0x2a -> $1515 0xb6,
  found by a one-off head-entry sweep also hitting 0x2c/0xb8;
  sweep deleted, 0x2a hard-pinned.)
- Else $152e < 5 -> 0xf12 (scan skipped, $152e untouched).
- 32-voice scan (stride 0x10 from 0x2800, byte compares): first
  +9 == 0xff with +5 == 0. Both dbf loops reload d7 (0x92e,
  0x97e, disassembled) -- no residue anywhere on this path.
- Miss -> $152e = 0 -> 0xf12.
- 0x95a arm: d4 = slot word; SCSP[slot+0x0b] = 0x1f,
  SCSP[slot] = 0x10, ram[0x2000+slot] = 0, voice+5 = 5;
  rebalance decrements +5 of EVERY 0xff-aged voice INCLUDING
  the just-armed one (5->4, stepper-pinned -- the armed voice
  does not skip itself); +9 never cleared here.
  $152e--, door 0xa8c (setup arm, S3).

## Legs (test_cdispatch.c, real ROM)

- loopb-miss ($152e 7, all +9 0): 203 steps to 0xf12, $152e
  7->0 both sides.
- loopb-arm ($152e 5, voice0 slot 0x0020/+9 ff/+5 0): 216
  steps to 0xa8c; voice0+5 = 04, $152e = 04, SCSP[0x20] = 10,
  SCSP[0x2b] = 1f, ram[0x2020] = 00, both sides.
- loopb-setup (ch 0xf, $1515 0x80): 73 steps to 0xf12 via 0x9b6.
- loopb-setup9ba (ch 0xf, payload 0x2a, $1515 0xb6): 70 steps
  to 0x9ba.
- S2c drain-90h legs rolled forward (early 0xf12 door): keep
  71 / table5 72 / clear 69; $1515 = 0x80 C==S; handler 0xf12.
- S2b drain-90-high/low legs: HIGH now runs to 0xf12 ($1515 =
  0x99 C==S, handler 0xf12, 72 steps); LOW still refuses at
  the split (0x858, 36 steps). Payload-vs-command discrimination
  (base 0x15) intact on both.

## Self-corrections in-slice

1. desc[13] re-read: nonzero files ITSELF and clears (bra past
   desc[12]) -- the first S2c draft had it backwards; silicon
   ($150d == ex-desc[13]) corrected it. Carried into the C.
2. Ternary precedence: `a ? x : b != 0 ? y : z` groups as
   `(a ? x : b) != 0 ...` -- worked by accident in S2c, failed
   loudly here; parenthesized.
3. Stale-RAM asserts: the C's clear path zeroes desc[13] before
   the step-pin assert reads it -- pin from the params.
4. replaceAll overreach: a bulk handler-pin edit converted the
   trap + LOW-split legs (still 0x858) alongside the synthetic
   HIGH legs (truly 0xf12); caught by the suite, reverted
   pairwise. Bulk edits across legs with different routing need
   per-leg eyes.

## Deferred (S2e backlog)

- 90 LOW path (0xa3e row-table walk to the F12 bra) -- still
  refuses at the split.
- B0/C0 heads (primer audit: d2 = payload at 0x13ca/0x1e5a
  too -- dedicated differentials, not a drive-by).
- Setup arms past 0xa8c/0x9ba (S3) and the 0xf12 compactor.
",
