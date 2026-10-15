# E1c recipe: the 0xed8 key-on arm (9/10 arc)

Status: STATIC MAP only (disassembly, no stepper run, no `src/`
change). Written while the E1b suite validation runs in
background -- no build, no tree mutation except this note, so
the running suite is undisturbed. Implement after E1b commits.

## Why this arm

E1b programs the slot words but KEYONB never sets (word0 =
0x1031, bit 0x0800 clear), so `slot_active` stays 0 on both
sides and render stays silent. L1 is key-OFF only (both arms),
L2 is a documented no-op. The real key-on is the `ori.b
#$18` RMW (KEYONB+KEYONEX, v0807 sweep: sites 0xd7e/0xefa/
0xf02), and the 0xefa site lives in the 0xed8 arm -- the
bit31-taken branch of the engine head that E1b refuses.

## Static map (disasm, +2 base)

- Head 0xc68-0xc7e (E1a, unchanged): `move.l $00(a2,d3.w),d5`
  / `btst #31,d5` / `bne 0xed8`. Gate bit = BE bit31 of the
  table+0 long (RAM 0x5000+d3.w).
- 0xd92 arm (refuse, unchanged): same slot program shape as
  E1b + two extra stream longs (+0x16/+0x12), then the dead
  copy loop at 0xdf8. Same silicon fate as 0xcb2.
- 0xed8 arm (THIS slice, 15 insns, all standard):
  - 0xed8: `andi.l #$fffff,d5` / `ori.l #$10300000,d5`
    (d5 = table+0 long; low-20 sample address + KEYON base)
  - 0xee4: `move.l d5,$00(a5,d4.w)` (SCSP slot long, d4 =
    slot word; same u16-pair form as E1b)
  - 0xee8: `move.l $08(a2,d3.w),d5` / `move.w d5,$04(a5,d4.w)`
    (table+8 low word to slot+4)
  - 0xef0: `move.l $04(a2,d3.w),d5` / `subq.w #1,d5` /
    `move.w d5,$06(a5,d4.w)` (table+4 low word minus 1 to
    slot+6; subq.w touches the LOW WORD only)
  - 0xefa: `move.b $00(a5,d4.w),d5` / `ori.b #$18,d5` /
    `move.b d5,$00(a5,d4.w)` (THE KEY-ON: slot+0 high byte
    |= 0x18; model `write_u8` to slot+0 fires
    `synchronize_slot` -> KEYONB set -> `slot_active=1`)
  - 0xf06: `lea $1000(a6),a2` / `move.b #$40,$00(a2,d4.w)`
    (stream byte ram[0x2000+slot] = 0x40)
  - 0xf10: `rts` -- CLEAN RETURN, no copy loop, no fault.
- Resume: the engine region runs inside the `jsr 0x81a`
  subroutine (drain 0x7f2 site), so `rts` lands at 0x7f6
  (`movem` restore, next descriptor). The C's 0x90 path
  already continues the descriptor scan on (OK, at_door=0)
  -- the arm plugs into the existing continuation.

## Inputs / effects (C signature)

Called from the engine-key head's bit31-taken branch with
(board, slot=d4 word, d3 index, d5=table+0 long): table base
0x5000; reads table+0/+4/+8 longs (bound: +12 <= RAM);
SCSP slot+0 long (u16 pair), slot+4/+6 words, slot+0-high
RMW via read_u8/OR/write_u8 (sync path, preserves rest);
stream byte 0x40. NO voice writes, NO 151x counters, d2
untouched. Returns VF2_OK, at_door=0 (continue scan) --
first engine path that completes a dispatch leg.

## Differential plan (mirrors E1b)

New leg in test_cdispatch.c: same S4 fixture, patch staged
table+0 |= 0x80000000 (bit31) before the S run; S from 0x7a6
to 0xf10 arrival (pin steps; expect OK, NOT unsupported);
step once more past `rts`, pin pc == 0x7f6 (resume proof).
C: full-RAM equality under the S4 exclusions + SCSP
equality INCLUDING `slot_active[slot]==1` on both sides.
Then render: peak > 0 (first live-keyed audible render from
a native dispatch -- S4b manual keying retired for this
shape). d3/table values staged (synthetic-gate precedent:
a8c-syn legs); the LOGIC is ROM-exact.

## Why the S4 shape takes the fall-through (sanity)

S4 table+0 = 0x00f00000 (bit31 clear) -> fall-through ->
dead loop. The bit31 flag marks a different sample-pointer
class (real one-shots vs staged silence); no live snapshot
exists, so the gate bit is staged. The arm's existence and
shape are ROM facts, not fixture facts.

## Sequencing

1. Background suite green -> commit E1b (+ this note).
2. Stepper-measure the arm (steps, resume pc, effects).
3. Implement arm + leg; full build + suite; commit E1c.
