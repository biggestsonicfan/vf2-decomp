# v0819 - S7a: drain class-switch audit + e0/A0 path legs

Status: new `vf2_m68k_cdispatch_probe` (10 legs) green; decoder
static-btst print fixed (ROM vectors); two C comments corrected
(no behavior change). One live C divergence reported, fix
deferred to S7b.

## Switch routing (7/7, exact predicted steps)

pc=0x81a reads filed $150e, masks $f0, beq-chain:
$90->0x858 @4, $80->0x10d0 @6, $e0->0x12b4 @8, $b0->0x13ca @10,
$c0->0x1e5a @12, $a0->0x1e74 @15, anything else->rts @0x856 @14.
The default rts is byte-verified (4e 75). This contradicts the C
dispatch_next default arm (->dispatch_90 + cursor/count consume):
ROM ignores unlisted classes with no effects and no consume. C
code untouched (needs the ring-consumer mapping, S7b); the false
"no ignore arm" comment is corrected in-tree with a DIVERGENCE
pointer here.

## e0 head legs

- no-match (ch 3, zeroed voices): 0x12b4 -> $1524/$4206 stores
  ($fff2, neg.w ran) -> 32-voice scan, no match -> rts 0x1354.
  164 steps. d1 ends 3 (filed channel), d2 ends $e9 (neg.b ran),
  no bus events. (Store cell is $4206 = $3200(a6), not $3206 —
  an a6 slip caught by the asserts, same class as R2.)
- gate (ch 9): bcc -> rts 0x13c8 @34. The C's "falls into the B0
  primer" gloss was +4-shaped; its early return stands (comment
  corrected to the rts mechanism).

## The bit-8 correction (decoder + C impact)

The e0 sign test is btst #8 (imm 8, ROM bytes 08 02/08 01), NOT
#0: the decoder masked static bit numbers to 3 bits for Dn forms
(silicon uses 5; the stepper was already correct — `bitword &
31u`). Fixed with ROM vectors (0x12ce Dn form + 0x227c memory
control). Consequences:
- d1 at 0x12fe is the TRANSFORMED index (combined>>5, $17 in the
  leg), and both negs key on its bit 8 (= payload bit 6), coupled
  (both or neither). The C dispatch_e0 even/odd rule
  ((transformed & 1)==0 -> negate) tests the WRONG BIT and must
  become a 0x100 test with the stale/index formulas re-derived.
  C code NOT changed here (needs the voice-match differential,
  S7b); this note is the work order.
- Instrument lesson, 7th instance: the stepper was right and my
  hand-trace was wrong because I read the print, not the bytes.
  The print is now fixed, closing this exact hole for Dn forms
  (memory forms were already correct).

## A0 primer verdict

Row indexed by command&$7f ($608e12, real ROM); max/row semantics
as the C models. Payload above row max exits clean via rts at
0x1eac (leg: idx $20 max $7f payload $80 @10 steps, d2=max). The
C's whole-path refusal (stale-a2 join) STANDS — and the clean
>max->rts arm is now a carved-out micro-admission candidate
(8 steps, no residue dependence: d2 is row-loaded). Not taken
(S7b, with the join analysis).

## Deferred to S7b

- e0 voice-MATCH + tail differential vs C dispatch_e0/e0_tail
  (the bit-8 C fix lands with it).
- b0/c0/90/80 arm paths + the default-arm C fix.
- Ring-consumer mapping (who moves ring -> $150e; cursor/count
  traffic) — the C dispatch_next consume model hangs on this.
