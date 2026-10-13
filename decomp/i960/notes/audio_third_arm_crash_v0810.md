# v0810 - Third arm + reachability: walk self-mutates desc[1], twin 0x260c unreachable (+4-era: entries shifted under +2; R3 re-visit pending)

Status: stepper-proven (loopb F8) + decoder vector. No `src/` change.

## Third arm at 0x25ee (measured)

Entry is mid-region but flag-clean (first instr overwrites d2):

- desc[1] bit7 clear: `bset #3,desc[0]`, desc[1] += 0x10, write back,
  `bra $265e`. Pinned: 0x25fa@3 … 0x265e@7, plus RAM effects
  (ram[0x2001] == 0x10, ram[0x2000] bit 3 set) and clean arrival at
  the 0x7a8 drain entry @13. The arm completes with no crash.
- desc[1] bit7 set: `bne` lands MID-INSTRUCTION at 0x260e (bodge:
  `ori.b #$41,(a2)` executes), then illegal `0x00ff` refuses at
  0x2612 (4 steps). Pinned: 0x260e@3 HIT + UNSUPPORTED @0x2612 @4.
- **Twin 0x260c has no entry.** No branch in the image targets it;
  the only neighbor entry (bne -> 0x260e) lands 2 bytes past it.
  Its body is correct code that can never execute — dead by
  construction, stronger than v0799i's "unmapped".

## Safe arm crashes downstream anyway (F8a/F8c2)

desc[1]=0 entry 0x25ee (and the F5 entry 0x2242 on record 2) flows:
epilogue ori.b (Z=0 always: mem|8 != 0) -> record advance -> empty
record -> `jmp $7a8` bypassing the count head -> 0x7a8 `bne` taken
(Z=0 residue) -> 0x7b2 bodge sled -> illegal `move.b a0,-(a2)`
(byte-size An source) at 0x7bc. Pinned: UNSUPPORTED @0x7bc @16
(F8a) and @30 (F5-continued), with full prefix HIT chains. The
0x1508 ROM bytes are pinned as `supported == 0` in test_decoder
(no decoder change was needed — it already refuses; the vector
locks the reason).

## Reachability argument (measured legs + open joints)

For the crash arms to fire on silicon ALL of these must hold:

1. Walk invocation with a suitable a0. PROVEN STRUCTURALLY:
   L2 vector 26 -> 0x1f16 driver -> `jsr 0x21bc` prescaler, whose
   `beq/bne $2242` forward on desc[0x0c]==0 / desc[0x0d]!=1. Boot
   leaves both zero (0x3cc writes +0x0e/+0x12/+0x16 only), so the
   boot-state prescaler ALWAYS forwards. BUT the prescaler never
   sets a0: the walk reads the PREEMPTED a0 (whatever the drain was
   scanning — a 0x4000 command descriptor during dispatch).
2. desc[1] outside {0, 0x80} at the preempted record. PROVEN
   AVAILABLE: boot command-descriptor images carry nonzero byte-1
   (img0 rows 01..08/0f, img1 rows 0a..0e/7f/..., read straight from
   ROM 0x6030e6/0x603186). AND the walk self-mutates: every safe
   pass does desc[1] += 0x10 (F8a RAM pin), arming its own next
   pass's crash.
3. $0409-bit7 clear at the three screens (else safe exits). OPEN:
   SCSP MIDI-area bit, meaning unmeasured (MAME cross-check only).
   The epilogue writes sibling $0408 (0x265c) — possibly a MIDI
   echo flow-control, hypothesis only.
4. L2 actually firing during the drain (interrupt enable/mask on
   silicon). OPEN: boundary, unmeasured in-repo.

So the crash SHAPE is proven (three sites: 0x22e8, 0x23fa,
0x7bc/0x2612-bodge) and two of four arming joints are measured;
production fire needs joints 3-4. No "game crashes" claim is made.

## Decoder hygiene note

Two hand-decode errors were caught during this slice by empirical
HITs (0x22e0 is a 4-byte move.l, not 6; 0x7bc is move.b a0,-(a2),
not d16). Lesson reinforced: never trust a length/print without a
stop-pc HIT — the sweep is the oracle, the print is a suggestion.
