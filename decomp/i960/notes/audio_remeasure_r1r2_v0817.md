# v0817 - R1+R2: L3 MIDI + walk/drain/epilogue re-pinned under +2

Status: `vf2_m68k_midi_probe` (3 legs) + `vf2_m68k_dispatch_probe`
(6 legs) green under the corrected base. No `src/` change. This
note supersedes the +4-era readings in v0813/v0814 (marked there).

## R1: L3 MIDI (test_midi.c rewritten)

- Leg 1 (0xFF reset): beq.w -> **0x5ec** (sr-disable, SCSP-quiet,
  SP reset, bra.w -> **0x100** vector-walk, signal, fault @0x10c
  on the deferred clr.l). 10 steps, a7=$5000. NO pushes anywhere:
  the 0x5ee push is dead (no referrers under +2) and the 0x102
  push was a +4 landing. a3/a2/scratch clean. The v0813 two-push
  analysis is retracted in full.
- New observable: **SR writes log bus events** (pre-existing
  2ea0e618 behavior). Leg 1 pins 3 events: SR@$5ec, SR@$100,
  signal $400001=0x1f. The old leg saw only the signal because
  the +4 path never executed an SR write.
- Leg 2 (data file): 0x54a bne.w lands **0x552**, so the subq
  always runs and the file arm needs **count 2, not 1**
  ([$2503]=2; count 1 takes the 0x580 arm). Exit via 0x5e2 store
  (no push) + restore + rte: OK @17 steps. The 16-register
  save/restore shuffle ($fff8/$1fff) is UNCHANGED (masks aren't
  PC-relative) and still pinned. a2 is reloaded from saved a5;
  the no-push evidence is the clean scratch, not a2.
- Leg 3 (wrap control): repurposed to direct 0x5ee entry with
  a3=0 (refuses UNSUPPORTED, 0 steps). The Dn->-(An) decoder
  support stays as capability coverage; its ROM need is gone.
- Protocol re-read: $1502/$1503 are remaining-data counters
  shared across one decrement-and-test (status bytes seed them);
  the v0813 "$1503 nonzero = running status" gloss was +4-shaped.

## R2: walk/drain/epilogue (test_dispatch.c rewritten)

- Every live entry lands on HEAD 0x2242 / drain-HEAD 0x7a6 with
  fresh flags. The circuit is flag-clean; the v0814 coupling and
  overlap are retracted (see v0816).
- **$0408 correction**: $0408(a5) with a5=$100000 is **SCSP+$408**,
  not a system port. The epilogue write targets the SCSP model
  (slot $408, in-range, silent): no bus event. Legs assert it via
  model readback, with a $AA pre-poison in leg 1 so the zero-write
  still proves execution. Any note language calling $0408 a port
  signal is wrong; only $400001 is the port.
- Leg 1 (zero record): HEAD -> drain-HEAD ((a4)=0) -> write ->
  beq TAKEN ($16==0) -> init-lea -> HEAD. 10-step self-loop,
  a0 re-parked. The advance arm runs only when $16 != 0.
- Leg 2 (advance, [$2016]=$07): write $07, beq nt, a0+=$20,
  HEAD. 4 steps. The 0x265e ori.b is mid-instruction dead bytes
  (inside the 6-byte move.b); the bit3 story is retracted.
- Leg 3 (two-record): $07-record advances, zero-record re-inits.
  20 steps, a0 home, SCSP cell ends $00 over the poisoned $AA.
- Legs 4-5 (prescaler TRUE): beq/bne.w -> **rts 0x2240** (2 and 4
  steps, $300d 2->1 pinned). The L2->walk link is gone; the TRUE
  prescaler is a countdown gate ending in return.
- Leg 6 (active record to advance): bmi -> 0x224c move.w/add
  (zero) -> bpl -> byte1 zero -> 10x ror pad -> $0409 screen
  (model zero) -> write $07 -> advance arm @21 steps, d0=d2=0.
- Scare resolved without a change: the first bisect of leg 6
  "looped" at 0x2274 for 20000 steps — that IS the leg-1
  self-loop entered via the active arm (fixture $16==0), not a
  stepper bug. Same-record infinite walk with $16==0 is the
  measured behavior; production record contents (S1) decide
  whether it matters.

## Deliberately deferred (R3+)

- v0808-v0810 active-arm sites under +2 (entries shifted; the
  $0409-set arms and byte1 != 0 arms are unmeasured).
- Drain body past 0x7b0 (kazuyette ran 3000 steps from 0x7a6;
  body pins are R3).
- Production walk driver (R4 — central open question).
- Voice-scan C re-recovery (kazuyette's R5; untouched here).
- The 0x580 L3 arm (count-1 path) has no leg yet.

## Lesson (instrument agreement, 2nd instance after v0816)

The ev=0/ev=3 confusion took three rounds because I kept the
v0813 "port" mental model ($0408 = $400008). Arithmetic first:
$0408(a5) with a5=$100000 is $100408, in the SCSP window the
stepper routes silently to the model. Register bases before
event expectations — the same class of error as the +4 base.
