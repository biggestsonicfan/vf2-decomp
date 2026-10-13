# v0808 - P2 twin-tail approach: desc[1]!=0 arm crashes at 0x23fa (measured)

Status: stepper-proven path map + decoder fix. No `src/recovered/`
change. The twin tails (0x24d2/0x260c) remain unreached — this slice
falsifies the route to them, it does not recover them.

## v0804 hypothesis falsified

v0804 said "the twin tails need a desc[1]!=0 fixture". Measured with
a desc[1] in {0x01, 0x80} fixture (entry 0x2242, a0 = 0x2000,
desc[0] = 0x80, zeroed tables, SCSP cleared): the run executes 45
true instructions and refuses at **0x23fa**, never reaching 0x24xx.
desc[1]!=0 is necessary but not sufficient; the crash comes first.

## True path (dense HIT chain, one step each, 8 -> 45)

0x2264 bne taken -> 0x2290 `ori.l #$66000020,d0` -> 10x `ror.l #8,d1`
-> 0x22aa `btst #7,$0409(a5)` (clear, SCSP zeroed) -> 0x22b0 `bne`
not taken -> 0x22b4/0x22b8/0x22bc pointer/counter loads (all zeroed)
-> 0x22c0 `cmpi.l #$200,d5` -> 0x22c6 `bls` taken (d5 = 0) ->
0x23b2 arithmetic sub-arm -> descriptor writebacks -> 0x23f6
`movem.l (a1)+,$00ff` (8 longs from RAM 0x2000, OK) -> 0x23fa trap.

Pinned in `vf2_m68k_loopb_probe` F6: 10 prefix HITs with step counts
(8, 9, 21, 22, 24, 25, 26, 27, 44, 45), 3 phantom-miss pins (0x22b2,
0x22b6, 0x23b0 — mid-instruction on the true stream), and the crash
verdict (UNSUPPORTED @ 0x23fa @ 45 steps) for both desc[1] variants
(0x01 and 0x80 converge identically).

## The crash: illegal-ea movem (ROM bytes 48 d2 00 ff)

0x23fa is movem.l reg->mem with ea mode 2 ((a2)): illegal
destination on Motorola (reg->mem takes predecrement/control dests
only). Silicon takes the illegal-instruction exception (vector 4 ->
0x4d4 crash stub, same class as the Loop-B 0x00FF verdict). The
stepper's UNSUPPORTED is silicon-equivalent. Escape routes (not
taken in this fixture): d5 = desc[0x0a] > 0x200 at 0x22c6 takes the
bulk-copy arm to 0x265e; $0409-bit7 set exits at 0x22b0. Whether
production state ever arms the crash (authored desc[1] with small
countdown and cleared status) is unmeasured — the fixture is
zeroed-table, not production.

## Decoder bug fixed (same class as v0799c/v0804)

The movem printer keyed only on class bits and always printed
`-(aN)` / `(aN)+`, so 0x48d2 misprinted as `movem.l $00ff,-(a2)`;
the true ea ((a2)) is illegal for the direction. Fix mirrors
`step_movem`'s ea rule in the decoder: illegal combos get
`supported = 0` + `.word` print (repo convention); legal prints
unchanged. Stepper observable identical (same status/pc/steps: the
refusal just moves from step to decode). Gate proven: pre-fix
binary fails exactly the 2 new asserts; post-fix green. ROM-byte
vector `{0x48,0xd2,0x00,0xff}` in `test_decoder.c`.

## C wiring boundary (unchanged, explicit)

No C function owns the 0x2242 walk (v0799i HELD stands): tails stay
unwired by design — a tail model without the walk driver is
theater. What this slice adds is the oracle-side path proof the
future walk-driver slice will consume (entry conditions, true
alignment, crash gate), plus one fewer decoder lie on its route.
