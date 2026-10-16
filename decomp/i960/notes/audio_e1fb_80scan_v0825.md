# E1f-b: 0x80 voice scan (9/10 arc)

Status: scan gates + in-memory `andi.b #$7f` + jsr door +
miss-all completion, two-round differential green in-file
(whole binary, 0 FAIL). Full suite in the commit validation.

## Recovery (`dispatch_80`, past the E1f-a row)

d1 = $150e & 0xf; 32 voices stride 0x10: +3==$1515,
+4==d1, +6 word==$1516, +2 bit4/1 clear; then `andi.b
#$7f,+2` IN MEMORY on every gate-passer (even beq-skips);
full pass refuses at the 0x11c0 jsr door (E1f-c); miss-all
falls to `return VF2_OK` (0x11ce rts resumes the drain, the
packet consumes -- second completed dispatch shape after
E1c). Shadowing fix: scan channel named `scan_channel`
(`channel` taken by the head; warnings-as-errors caught it).

## Differential (E1f-a row fixture + staged voices)

- Round A (voice0 +2=0x80/full match, voice1 +2=0x40/full
  match): C refuse@0x11c0, v0+2=0x00 (andi fired, beq
  skipped), v1+2=0x40 (jsr taken); S arrives 0x11c0 in 83
  steps with sv0+2=0x00.
- Round B (voice1 unmatched, voice2 +2=0xc0/+3=0x00):
  C OK, handled 1, count 0, cursor +4, v0+2=0x00,
  v2+2=0xc0 (early miss keeps bit7); S exits 0x265c in 309
  steps; full RAM under the drain exclusions + SCSP equal.

## Lesson (scan-order entanglement)

First draft staged the andi witness AFTER the match (voice3
vs voice1): both sides stop at the FIRST jsr, so the witness
never ran (v3+2=0x80 both sides, correctly). Witnesses must
precede the match in scan order. The 72-step first reading
was the old layout (voice0 first-gate miss); the andi+beq
pass costs exactly 11 more (83).
