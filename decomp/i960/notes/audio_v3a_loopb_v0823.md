# v0823 - V3a: Loop-B re-pinned under +2, WILL_FAIL removed

Status: `vf2_m68k_loopb_probe` green WITHOUT `WILL_FAIL` (all
~20 legs). Stepper-only slice (no `src/` change): every pin is a
measured stop-pc/step count against the fixed decoder. Both
`WILL_FAIL`s from the pcrel merge are now gone (triangle v0821,
loopb here).

## What flipped vs the +4 era

- Loop-B (0x902) is a live 32-voice scan (F1: 132 steps to the
  0xf12 exit; F3: 141 with the 9-insn head). The 0x934 trap /
  0x00FF word / ori.b sleds were mid-instruction landings.
- Drain entry is 0x7a6 (F4: 2 steps); 0x7a8 is interior (bne).
  The 0x7bc/0x7b2 "bodges" were the same artefact class --
  0x7bc is `move.w $1508(a6),d0`, 0x7b6+ is a legal descriptor
  scan, and 0x7f2 is the LIVE command switch (90/80/e0/b0/c0/a0
  classes + rts default -- the production packet pump).
- The v0808-v0812 "crash" family mostly evaporates: F8/F9/F10
  legs run, loop, and return. What SURVIVES: the two
  illegal-ea `movem` stores ($48d2 bytes) at 0x22e8 (F7: 21
  steps) and 0x2532 (F6: 23/35 steps by desc[1] arm).

## New pins (all first-try or ±1-adjusted predictions)

- F5 walk-active: bmi/bpl taken, bne not, 10x ror, btst/beq to
  the epilogue, 19 steps to 0x265c.
- F6 bls-taken chain via 0x23b0/0x24be/0x2522 to the 0x2532
  trap (23 steps; 35 via the 0x80 ror fall-through).
- F7 bls-fall-through chain to the 0x22e8 trap (21 steps).
- F8a three live passes (third arm -> epilogue -> 0x24d2 arm
  clears byte0 -> drain): 54 steps to 0x7a8; desc[1] = 0x10,
  byte0 = 0x00 mutations pinned.
- F8b twin arm: 26 steps to 0x7a8.
- F8c drain passage: 0x7a6 @53, 0x7a8 @54, 0x7ac @55, then the
  drain/epilogue livelock (frontier).
- F8c2 idle fixed point: the epilogue resets a0 = $1000 (ring
  bytes as record), so every lap is the 6-step drain-idle
  passage; pinned as two identical 6-step laps.
- F9 drain contract: count=1 full 20-iter scan + cursor advance
  (98 steps to 0x816, a7 balanced); count=0 idle (3 steps).
- F10 full L2 tick: 7 head + prescaler ret + 8 workers
  (1 bare + 7 lea-bound) + movem restore = 51 steps to the
  0x1f7a rte, a7 balanced, no events.

## Convention nailed down

`done` excludes the stop instruction (arrival count), which is
why hand counts kept landing ±1. Stated in-test nowhere yet --
the sweep asserts ARE the convention.

## Remainder

- Voice-start C recovery (0x9ba/0xa8c arms: $1516 tempo?,
  descriptor authoring, volume mulu/lsr/not math) is V3b.
- Render/WAV probe with manual key-on is V3c.
- The 0x49e `$1526` dance question (v0822) still open.
- docs/UNCOVERED_BRANCHES v0804 pointer (v0821) still stands;
  the R6 full correction pass is untouched.
