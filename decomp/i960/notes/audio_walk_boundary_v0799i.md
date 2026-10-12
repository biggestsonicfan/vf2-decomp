# Walk boundary: 0x2242 descriptor walk + twin activation tails (HELD)

## Shape

A descriptor-walk function spans roughly [0x2242, 0x2672]:
descriptors stride $20 (`adda.w #$20,a0` at 0x2666, reset
`lea $1000(a6),a0` at 0x266e), loop-back `jmp $2242` at
0x266a/0x2672 in the verified exit chain. Interior: movem bulk
copy loops, flag-dispatch branches, arithmetic paths, and two
twin activation tails.

## Twin tails (0x24d2 / 0x260c, same body)

With d1 = desc[0x12] zero-extended, `lsl.w #4` (slot word):

- SCSP[slot+1] RMW: read, `andi.b #$9f` (clear word bits 13-14,
  KEYONB in byte 1 bit 3 preserved), write back. The C-side
  primitive exists (`write_u8` routes slot+1 through
  `synchronize_slot`, whose activation edge fires only if KEYON
  is set while inactive) but the path is unwired, so the edge
  behavior on this route is untested either way.
- `move.w desc[0x1e],slot+6`; `desc[0] = 0` (consume flag).
- `lsr.w #1,d1`, table `lea $1800(a6),a1`: `btst #3,$02(a1,d1)`;
  set arm `addi.b #$50,$0b(a1,d1)`, else `addq.b #3,$0b(a1,d1)`;
  both `bset #0,$02(a1,d1)`; exit `bra $265e`.

Entries: 0x24d2 falls through from the 0x24c8
`btst #7,$0409(a5)` / `bne $265e` screen (d3 != 0 arm);
0x260c's outer entry is unmapped (the 0x2600-0x2608 head exits
via `bra $265e`, nothing falls into 0x260c). 0x24be's own entry
is unmapped (0x24ba `bra $265e` skips it; 0x24bc-0x24bd 2 bytes
unread). 0x264e-0x265b status unknown (jumped over by the
0x264a bra; entered-from-elsewhere unverified — not cited).

## Verdict: HELD, no C call site

Nothing in `sound_board.c` / `sound_board.h` references
0x2242/0x24d2/0x260c/0x265e/0x23b0; the C pump covers dispatch
+ voice/stream maintenance only. Recovering the tails now would
be an uncalled model (theater, per v0799d). The future slice is
"model the 0x2242 walk", which needs entry mapping (0x24be,
0x260c, 0x264e unknowns above) and residue audits first — not
the tail bodies, which are already decoded here.
