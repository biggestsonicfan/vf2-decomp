# E1q: drain-delivered LOW author (first dispatcher-fed LOW proof)

Status: new `test_low_author_drain` differential green
in-file (whole binaries, 0 FAIL). No `src/` change -- the
LOW branch was already recovered (S3c); this slice proves
it reachable from the dispatcher. Full suite in the commit
validation.

## What was open

The LOW author (`setup_author_low`, ROM 0xb76) had exactly
one differential: `test_a8c_synth` low, a DIRECT
`setup_author()` call with staged registers and staged
RAM. No drain-delivered command had ever been observed to
reach it, so the "0xb76 LOW fill reachability" item stood
as likely-permanent. It is not: the ordinary 90-LOW walk
-> allocator-exhaust -> reaper-found chain lands on it
with walk-filed (not staged) inputs.

## The chain (532 steps, drain 0x7a6 -> 0xcb2)

90-LOW walk (desc[2] 0x05: files $1516 = 0x0090,
$1518 = 0x00, $1515 = 0x27, $1511 = 0x07 -- the same pins
as the 90-low leg, re-asserted here) -> 0xf12 exhaust
(all +2 = 0x40, cursor 0x2800 untouched) -> 0xf50 finds
voice0 (+9 == 0, slot 0x0020) -> key writes, voice+2 = 0,
ram[0x2020] = 0, rebalance (all 0x40 -> 0x3f, voice0
0 -> 0xff), $151e 0 -> 0xff -> bra 0xa8c ->
LOW author (desc[2] < 0xf0) -> +9 = $151e = 0xff
($151e++ -> 0, stepper-pinned) -> sample tail (zeroed
$5000 gate: fall-through) -> refuse at 0xcb2, packet held.

## Voice-shape cross-check

voice0 lands 40/00/ff/27 on both sides: +2 = 0x40 with
NO bset chain (HIGH lands 0x58), +5 = 0 (no $152e
traffic on LOW), +9 = 0xff, +3 = filed $1515. The
HIGH/LOW structural differences from the S3c comment
reproduce exactly under drain filing.

## Remaining sound work

0xf12-found arrivals (E1n refuse, stands); 0xfb0 variant
(unreachable, E1p); 0xf50-exhausted rts return semantics
(arrival-only pin); live gameplay writers; DSP-absence
sweep. The LOW branch is now dual-provenance (direct +
drain); the HIGH branch already was (arm + reaper).
