# E1n: 0xfac d4-as-slot verdict -- the allocator path can't enter (9/10 arc)

Status: `test_fac_d4_crash` two-phase differential green
in-file (whole binary, 0 FAIL). No recovery change (one
comment line): the E1m refusal at 0xfac is UPHELD, now with
teeth. Full suite in the commit validation.

## The question

The allocator (E1m) bras to the 0xfac setup authoring,
whose slot stores use d4-as-slot. Core-fill arrivals (S3
future, via 0xa8c) carry d4 = voice slot (loaded at 0xa92)
and are sane. What does d4 hold on allocator arrivals?

## Measurement

Two-phase S on the 90-low fixture (walk index 0x7f):

- Arrival at 0xfac (67 steps): d4 = 0x0000007f. The walk's
  $1515 index, low word exact, upper drain residue (0 on the
  harness). a1 = 0x2810 (allocated voice1), a2 = 0x2820
  (advanced cursor), a0 = 0x4000 (descriptor) -- everything
  else sane. Static audit confirms no d4 writer exists
  between the walk (0xa3e) and the slot stores (0xafa-0x103e
  verified instruction by instruction).
- Continue: the stepper sails to 0xcb6 in 114 steps
  (UNSUPPORTED at the dead loop's illegal movem).

## The three-way split nobody predicted

1. The stepper has NO address-error model: the odd word
   store at 0x103e (`move.w d6,$10(a5,d4.w)` with d4.w =
   0x7f) executes silently. Oracle gap, recorded -- a future
   stepper slice may model 68k address errors, but no
   recovery may depend on that behavior either way.
2. Real silicon takes vector 2 -> ROM 0x6004d8 (`moveq #7,
   bra 0x4fe` blink loop -- read off the word-swapped image,
   not assumed): the odd-index allocator path BLINKS, dead.
   ROM-proven, not merely architectural.
3. Even walk indices would execute (no alignment fault) and
   program slot-by-index garbage -- deterministic but
   meaningless, modeled by neither side.

So the C correctly refuses at 0xfac for allocator arrivals
under ALL parities (crash on odd, garbage on even), while S3
will enter 0xfac with d4 = voice slot on the core path. Same
address, two provenances, opposite verdicts -- the handler
pin (0xfac) is now a fork, not a door.

## What this slice is not

Not a recovery (nothing new admitted), not a stepper fix
(the gap is documented, not patched), not a 0xfac
completion (the core path with sane d4 is S3's job).
A verdict with a pinned oracle-gap demonstration.
