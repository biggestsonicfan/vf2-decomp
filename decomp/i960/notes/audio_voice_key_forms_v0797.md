# v0797 - voice key forms (V1-V5) + decoder mode-6-#imm fix + v0796 amendment

## Decoder bug (print-only, length correct, now fixed)

`move.#imm,(d8,An,Xn)` printed the raw brief-ext word as a d16
displacement and dropped the index register: `move.b #$1f,$0b(a5,d4.w)`
(ext 0x400b) printed as `move.b #$1f,$400b(a5)`. Only the MOVE-#imm
handler merged modes 5/6; ADDI already printed brief-ext correctly.
Lengths were always right (mode 6 = 2 ext bytes either way), so no
disassembly ever misaligned; only text for this form lied.

Blast radius in C (both fixed this slice):
- `sound_board_key_off` (control-bit RMW fiction) and
- `sound_board_clear_c0_voices` (absolute SCSP+0x0b/+0x00 +
  ram[0x2000] instead of d4-indexed).
Init/boot/drain/tail code used non-imm sources or d16 forms and
re-verified clean against the fixed decoder (0x28e in full; e0 tail
0x1356: `move.w d0,$4010(a5)` + `move.w d2,$401a(a2)` both d16-absolute
as the C has them).
Fix: split mode 6 to a proper brief-ext print with the DA bit
(`$%02x(a%u,%c%u.%c)`). Unit vectors: 0x1BBC + 0x001F/0x0010 +
0x400b/0x4000/0x100b (ROM bytes from 0x95e/0x964/0x688). ctest
vf2_m68k green.

## Voice key forms (all dN = voice[0] slot word, slot*0x20, no mask)

- V1 (L1 ISR 0x636/0x684): SCSP[d+0x0b] = 0x1f; ram[0x2000+d] = 0.
- V2 (primer-tail 0x95e, loop-C 0x1d88/0x1c38, loop-D 0x1e28):
  SCSP[d+0x0b] = 0x1f; SCSP[d+0x00] = 0x10; ram[0x2000+d] = 0.
- V3 (primer-tail second scan 0x9f4): SCSP[d+0x00] = 0x10;
  ram[0x2000+d] = 0 (no +0x0b write).
- maintain_voices == ISR loop A exactly (32-record scan, bit0 gate,
  --[0x0b] gate, bit3 split, counters, both rebalance scans with the
  word-compare direction as the C has it), modulo key_off (now V1)
  and the d2-residue question below.
- C0's 0x1de0 body == loop D (V5 writes + clears + unconditional
  --151e, no rebalance) with the 0x3200/0x3220/0x3240 clears and the
  $150e&0xf channel gate as the C has them.

## Interrupt map (68k vectors, ROM view +0x600000)

- L1 (0x5fe): movem spill -> 0x602 voice tick (entry $041a = 0x3c0,
  $0422 = 0x80) -> rte. game.c's per-frame maintain_voices ~= tick.
- L2 (0x1f16): spill, $0418 = 0xce / $0422 = 0x40, $1506 != 0xff
  gate, jsr worker(0x2000) + 8x desc tick. Driver pokes are
  unmodeled-port writes (same category as $400001/$0405).
- L3 (0x526): MIDI-byte receiver ISR -> rte at 0x5ea. Reads $0405,
  runs a $1500-$1503 framing state machine into the $1500 ring
  ($1504 cursor & 0xfff, $1506 count), jsr 0x49e ($1526-toggle +
  $400001 poke). This is the real command-fill path (main CPU ->
  MIDI -> ring -> drain); the C's emit_command is its modeled
  equivalent, the byte-framing seam unmodeled: boundary.
- L4-L7/spurious (0x4c4-0x4fc stubs + 0x5ec reset stub): moveq irq#
  -> $400001 poke + spin (or stack-reset + bra $102). No C impact
  (no irq model).
- 3 rte in the image: 0x5ea (L3), 0x6e4 (L1), 0x1f7a (L2).

## v0796 amendment (correction, code unchanged)

- "No static entry => dead" was wrong for an ISR: the L2 driver IS
  entered (vectored) and its worker-forward (0x21ba +0x0c==0 ->
  0x2242 main-loop head with the preempted a0) is the live path.
  Unmodelable in C (needs preempted register state): boundary.
- The countdown/emit half stays dead: +0x0c is boot-cleared and no
  store of nonzero to any stream desc +0x0c exists (d16-dst scan:
  only primer READS at 0x8d2/0x8e4, a 0x3000-table RMW at
  0x196c/0x19a8, and the F-path +0x0e store; indexed-dst 0x0c hits
  are bit-ops/reads/coincidences). Worker self-disarms at 0x220e.
- maintain_streams no-op + deleted worker fiction: upheld, reason
  corrected (arming dead, not entry dead).

## Known unknowns (documented, not modeled)

- d2-residue in the rebalance word-compare (move.b age over
  interrupt-residue high bytes). Modeling needs the guest register
  file; no 68k oracle exists. C keeps clean age.
- Loop B (0x902 primer tail, $150e&0xf/$152e gates) and the
  0x1c0a B0-sub-dispatcher + 0x1c24/0x1d5e scans: post-refusal or
  dead-entry territory (only B0 0x3a live; 0x1d5e entered once via
  beq from the 0x7e arm). Future 90/80-voice targets, not this slice.
- 0x3000-table loops (0x1958/0x1986, no entry found) and second wave
  uploader (0x19e2, d0-gated, 0x500 bytes (a2)->SCSP+0x700):
  mapped-unknown, beyond refusal.

## Decoder gap noted (not fixed)

0x49e contains a `movep`-shaped pair (`.word $0007/$0010`) the
decoder does not implement. Cosmetic here (surrounding code reads
clean); left for a decoder slice.

## Gates

1. key 0x1f->0x1e: 2 FAILs (both V1 asserts). 2. c0 0x1f->0x1e:
   1 FAIL (indexed assert). Decoder vectors: old print observed
   ($400b(a5)) vs new expectation ($0b(a5,d4.w)); vf2_m68k green.

## File map

- `src/m68k/decoder.c`: mode-6 #imm print split.
- `tests/m68k/test_decoder.c`: 3 ROM-byte vectors.
- `src/hardware/sound_board.c`: key_off V1, c0 V5-indexed, ISR/L2 +
  residue comments.
- `tests/hardware/test_sound_board.c`: V1/V5 asserts (indexed;
  untouched bytes pinned: ram[0x2000] = 0xaa, SCSP[0] = 0xff).
