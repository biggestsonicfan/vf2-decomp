# E1a: engine key-block map + fault contract (9/10 arc)

Status: `test_engine_probe` green (S arrivals + fault pinned,
C door pinned, evidence table printed). The copy loop is
dead-on-silicon; the C will refuse at 0xcb2 by design (E1b).
Full suite in the commit validation.

## Map (68k 0xc68 region, S4 packet)

- Head 0xc68-0xc80: table pair at (a2+d3.w) (`move.l`,
  bit31 -> 0xed8), slot word at +0x04, gate word at +0x0c
  (!= 0 -> 0xd92). Falls through for the S4 shape.
- Slot program 0xc82-0xcaa (11 steps): KEYON long
  0x10300000|d6 to SCSP slot, sample long d5 -= 0x200 to
  stream+0x0a, 0x10 to +0x01, rate pair (d2, d2+0xeba) to
  +0x1a/+0x1c. All standard instructions, all stepped.
- Copy loop 0xcb2+: unrolled `movem.l (a0)+,d0-d7` /
  0x48d2 / `adda.w #32,a2`. The 0x48d2 word decodes as
  movem.l d0-d7,(a2) -- ILLEGAL on silicon ((An) is not a
  legal movem-reg->mem dest). The vector-4 handler at 0x4d4
  funnels (with d0 = 8) to the 0x4fe fault loop, which masks
  interrupts and blinks the code on $400001 forever. Any
  live path reaching 0xcb2 crashes the sound CPU: the loop
  is dead code in practice, and every source bit must avoid
  it. Stepper contract: UNSUPPORTED at 0xcb6, 384 steps
  (movem-load at 0xcb2 succeeds off real banks; needs the
  8MB window -- a 3MB window faults the LOAD instead).

## Entry-state evidence (0xc82, 372 steps)

d2 = 0x1004 (rate word, derivation OPEN -- loaded in the
setup arm, rate table at 68k-absolute 0x600c48 not in the
audio region); d3 = 0x2f80 ($5000 table index, derivation
OPEN -- setup arm); d5 = 0x4a62 = BE32(RAM[0x5000+d3+4])
DERIVED; a0 = 0xF00000 = BE32(RAM[0x5000+d3]) DERIVED
(sample-bank pointer; needs the 8MB window to step);
d6-entry 0, d6-exit 0x10310000 = 0x10300000|BE32(voice+0x0c)
DERIVED; d4 = 0 = voice slot word DERIVED; a1 = voice ptr,
a2-entry = 0x5000 table base; a3-entry (0x60d066) dead
(overwritten by lea, unused).

## E1b firewall

The C recovery needs d2/d3 derivation rules from the
setup arm (0xa8c -> ... -> engine edge). Without them the
slot program cannot form inputs honestly: derive or keep
the 0xc68 door. Next dig: setup-arm register flow for
d2 (rate) and d3 (table index) on the S4 shape.
