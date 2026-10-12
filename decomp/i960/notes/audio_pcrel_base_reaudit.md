# 68000 PC-relative base: correction and first re-audit (draft, not yet reviewed by the maintainer)

## Finding
`src/m68k/decoder.c` and `src/m68k/stepper.c` used `opcode address + 4` as the
base of PC-relative forms. On the 68000 the base is the address of the extension
word (`opcode address + 2`). Measured on `epr-17574.30` (0x180..0x2700, decoder
instruction boundaries; JSR/JMP up to 0x4000): with +2 every target lands on an
instruction start (Bcc.w 176/176, BRA/BSR.w 42/42, DBcc 67/67, JSR/JMP (d16,PC)
183/183); with +4 only 31, 4, 19 and 125 do.
`tests/m68k/test_pcrel_base.c` pins the rule without a ROM (it fails on the old code).
The jsr/jmp (d16,PC) +4 came in with 2ea0e618; the Bcc/DBcc +4 dates from 0f18de0a.

## Effect on earlier statements (re-audit in progress)
- Drain entry: 0x7a6 (`move.b (a4),d0`), not 0x7a8. The old trap at 0x7bc
  (`move.b a0,d0`, "move-src mode-1 gap") was a mid-instruction landing.
  From 0x7a6 the stepper now runs 3000 steps without refusing.
- Loop-B (0x902) with the corrected decoder is ordinary code, not a sled:
  a 32-iteration scan (`moveq`-free: `move.w #$1f,d7`; `adda.l #$10,a1`;
  `dbf d7,$932`) over records at `$1800(a6)` testing bytes +9 (== 0xff) and +5
  (== 0), then SCSP-window writes through a5. `dbf` lands on 0x932, the start
  of `cmpi.b #$ff,$0009(a1)`; `bne` lands on 0x946 (`adda.l`); `beq` on 0x95a.
  The old "dead" verdict (trap at 0x934, sleds at 0x948/0x95c) came from the
  +2 error. Stepper from 0x902: 133 steps, then refuses at 0xcb6 on
  `movem.l d0-d7,(a2)` (stepper coverage gap).
- Stepper also refuses `addq.w #4,$1508(a6)` (0x804): ADDQ to memory, a
  coverage gap, found from the drain body (entry 0x7b0).

## Not re-audited yet
A0 sled rows, 0x2242 walk entries (0x24be/0x260c/0x264e), the "seventeen dead B0
handlers", the 90 allocator removal, and every note that quotes a branch/jsr
target printed by the old decoder (all such `$xxxx` are +2 too high).

## Consequences for the C recovery and the two pinned tests
- `vf2_m68k_triangle_differential` and `vf2_m68k_loopb_probe` pinned the
  old behaviour ("voice 0 visited 32 times, voices 1+ frozen", Loop-B sleds).
  Under the corrected base the stepper walks the voice records normally and
  the C (`sound_board.c` voice scan) diverges from it (e.g. voice timer
  0x0280b: C=0x08, stepper=0x27 in s1-clean). The "frozen voices" model is a
  decoding artefact, so the C voice scan needs re-recovery, not just new pins.
- Until then both tests are marked WILL_FAIL in CMake (known divergence,
  visible, flips when fixed). Nothing else in the suite depended on them.
- Independent check (MAME, real 68000 sound program, ~2450 attract frames):
  the hot idle/tick loop at 0x1f88 (2112/2450 PC samples) is the target of
  `bne` at 0x1f82, which only resolves to 0x1f88 with the +2 base. Zero
  reads/fetches in 0x932-0x99e (Loop-B) over ~8.1 M tapped accesses, i.e.
  not exercised in attract (not generalised beyond it). PC sampled between
  timeslices is not always an instruction start, so only the branch-target
  test is evidence.
- Open: stepper coverage gaps (movem to memory, addq to memory d16(An)).
