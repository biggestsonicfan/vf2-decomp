# v0814 - true-dispatch hunt: the walk circuit is flag-coupled

Status: stepper-proven dispatch map + 7 legs (new ctest
`vf2_m68k_dispatch_probe`). No `src/` change. The v0808-v0812
crash sites are now explained as reachable via the NORMAL walk
circuit, not just fixtures.

## Byte-verified dispatch map (ROM bytes, not prints)

| entry | callers (byte-verified) | status |
|---|---|---|
| walk-HEAD 0x2242 `move.b (a0),d0` | prescaler 0x21be beq.w, 0x21c6 bne.w | TRUE, fresh flags |
| walk-MID 0x2244 `bmi` | boot 0x21e jmp; epilogue 0x266a/0x2672 jmp | TRUE, stale flags |
| drain-MID 0x7a8 `bne` | walk 0x2248 jmp, 0x225c jmp | TRUE, stale flags |
| drain-HEAD 0x7a6 `move.b (a4),d0` | none (re-confirms v0811 no-caller) | dead |
| prescaler-TRUE 0x21ba `tst.b $c(a2)` | none (no jsr, no vector, 0x21b8 rts blocks fall-through) | dead |
| prescaler-bodge 0x21bc (mid-tst) | L2 0x1f36 jsr | live (re-confirms v0812) |
| epilogue-init 0x266e `lea $1000(a6),a0` | none: 0x265e `ori.b #$8,(a6)` can never set Z, beq $2670 never takes | dead |
| boot-first 0x21e `jmp $2244` | boot corridor (a0=$1000(a6) set at 0x1d4) | TRUE first dispatch |

Slots $2000..$20e0 and records $1000(a6) stride $20 are the same
window at a6=$1000. Boot parks a0 on record 0 and jumps to MID.

## The flag coupling (v0811 mechanism found)

- Record 1 via HEAD: fresh Z from `(a0)`. Inactive -> drain-MID
  with Z=1 -> bne nt -> epilogue. Clean (leg 1: 5 steps).
- Epilogue: `ori.b #$8,(a6)` forces Z=0 (bit3 set, always), N =
  bit7 of the header byte. beq never takes; a0 += $20; jmp MID.
- Record 2+ via MID: bmi tests STALE N, drain bne tests STALE Z=0
  -> TAKEN, always. The drain's arm selection is driven by the
  epilogue's residue, not by record contents. Leg 6 proves it.

## The 0x7b2 overlap arm (all v0808-v0812 crashes explained)

bne taken -> 0x7b2 = mid-movem bytes, executed as overlapping code:

- 0x7b2 `ori.l #$531447ee,d0` (6 B: consumes the movem mask +
  the subq bytes as immediate),
- 0x7b8 `ori.b #$2e,d0` (4 B),
- 0x7bc `move.b a0,d16(a2)` = the silicon-illegal An-byte-source
  form -> stepper refuses UNSUPPORTED (existing 0x1508 vector
  behavior, still correct).

Pinned (leg 6, all-zero records): fault @0x7bc @14 steps,
d0=$531447ee (0 | imm), a7=$7f00 (movem push SKIPPED, stack
stays balanced), a0=$2020. On silicon this is an illegal-
instruction exception -> vector 4 -> 0x4d4 (`moveq #8,d0; bra
$4fe` halt). The stub/halt chain byte-verified (vecs 24-31 all
land 0x4d4-0x4fe family; L1/L2/L3 = 0x5fe/0x1f16/0x526).

## Sharpened production paradox

Record 2+ ALWAYS takes the overlap arm (Z=0 unconditional), so a
multi-record walk can never proceed past record 2 in ANY state.
Yet boot dispatches the walk and L2 ticks it. Open joints:

1. a0==a2 convention? L2 parks a2 on the slot; if the main loop
   parks a0 on the same slot, each tick walks ONE record via
   HEAD-fresh and the epilogue advance is re-parked next tick.
   Needs main-loop a0 analysis (next slice).
2. TRUE prescaler (countdown per slot) is dead; the live bodge
   walks every tick. Why keep the countdown? Possibly the bodge
   is itself a patch-era artifact.
3. Single-record sound design? The 8 slots x $20 with per-slot
   countdowns suggest polyphonic streams, not a record loop.

## Phantoms killed this slice

- "0x2233 br8 -> walk-MID": byte 0x6E mid-`andi.b #$ff,$1504(a6)`
  (0x2232, 6 B). Dismissed; MID callers are boot + epilogue only.
- "0x36116 jsr (a5)": surrounding words unsupported; data phantom
  in a high region (outside audio code). Dismissed.
- Main-dispatch 0x12d2/0x12d8 `bne.w $12e0` land MID-INSTRUCTION
  (0x12e0 = inside `move.b $0007(a0),d0`). Overlapping-code arms
  in main-loop territory: recorded, NOT pursued (needs main-loop
  state; separate slice).
- L3-lesson holds (4th instance): every linear print re-checked
  against bytes before trusting; the 0x224e-0x2264 chain HIT
  sequentially (prints TRUE there), d0=$80<<2=$200 measured.

## Legs (`vf2_m68k_dispatch_probe`, all green)

1. inactive HEAD -> epilogue @5 steps, a0 untouched.
2. epilogue advance @4 steps, a0 $2000->$2020, header bit3 set.
3. full inactive loop exhaust @9 steps back at MID, a0=$2020.
4. prescaler-TRUE counter-clear -> HEAD @2 steps.
5. prescaler-TRUE countdown ($200d 2->1) -> HEAD @4 steps.
6. record-2 overlap fault @0x7bc @14 steps (d0/a7/a0 pinned).
7. active record-1 -> epilogue @21 steps (d0=$200, d2=0).
