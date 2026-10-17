# Slice-6 DSP: complete SCSP 0x400-0x43F write set — no DSP program (v0804)

## Method

Raw scan of the audio region (CPU view, word-swap accounted for) for
every MOVE (any size, either operand) and every imm-op
(ori/andi/subi/addi/cmpi/eori) touching `(d16,a5)` / `(d8,a5,Xn)` with
displacement `0x400-0x43F`. 18 MOVE hits + 8 imm-op hits, each
triaged against disassembly context.

## The complete write set (7 addresses)

| addr | writers | status |
|---|---|---|
| 0x400 | boot `0x184` move.b #imm | modeled (C boot mirror) |
| 0x401 | boot `0x20c/0x28e`, L1 `0x5f0` move.b #imm (= 0) | modeled |
| 0x402 | boot `0x2ac` move.w #imm (= `0x138`) | modeled (constant kept; function unidentified, see below) |
| 0x408 | `0x265c` move.b (skipped by `0x2658 bra`, no xref entry) | DEAD letters |
| 0x418 | boot `0x2f4`, seq `0x1f1a` move.w #imm | modeled |
| 0x41a | boot `0x2fa`, L1 `0x602/0x656` move.w #imm (`0x3c0`) | modeled (L1 timer/control poke) |
| 0x422 | boot `0x300`, L1 `0x608`, seq `0x1f20` move.w #imm | modeled |

Reads: `0x405` x5 (boot MIDI drain + L3 `0x52a` MIDI input) — input,
not DSP programming.

No writes exist anywhere in `0x403-0x407`, `0x409-0x417`, `0x419`,
`0x41b`, `0x41c-0x421`, or `0x423-0x43F`. No ori/andi/eori (or any
imm-op) targets the DSP window at all — only `move.w #imm`.

## Verdict

A DSP reverb engine needs sustained traffic across the DSP window
(ring config, per-slot effect sends, program steps). Observed: a
handful of boot constants, two L1 timer pokes, MIDI reads, and one
dead letter. The C's PCM-only model covers every live write.
`0x402 = 0x138` keeps its constant without a semantic name (a DSP
program address is possible but unproven; inventing it would violate
fail-closed).

DSP effects: boundary for lack of evidence, upgraded from P5's open
TBD to a closed write-set inventory. FM: unchanged (not used by VF2).
No `src/` change.

## Left open (deliberately)

- A0 applier sled audit (64 valid rows into `0x8f12-0x9ba2`
  sled-riddled bodies, e.g. row 0 = `ori.b #$0,d3` then an illegal
  word; rows 64+ read code-as-pointers and would crash). C's
  refuse-at-entry stands on two legs now (residue-d2 split AND
  sled/crash targets). Zero A0-table entries in `0x880-0x1100`.
- 80-LOW mid-lea arm and 90 `beq $10e4` mid-80 entry: same
  stepper-trap-or-sled + xref + matchability method as Slice-1.
- move-src mode-1 gap in decoder/stepper (blocks drain tracing
  past `0x7bc`).

## Correction (v0826)

Superseded by `audio_dsp_program_measured_v0826.md`.

1. **Title/verdict "no DSP program" is wrong.** The scan scope
   (`0x400-0x43F`, `(d16,a5)`/`(d8,a5,Xn)` forms) could not see the DSP
   register window `0x700-0xBFF`, which is exactly where the sustained
   traffic lives: the boot uploads a real `0x500`-byte DSP program
   (ROM `0x28e`; blobs `0x604948`/`0x604e58` = COEF 64w + MADRS 32w +
   128 MPRO steps; LastStep 82 / 44; EFREG writes at steps 45/47/79/81
   and 41/43) and configures the delay ring via `0x402 = 0x138`.
2. **`0x408` is not a dead letter.** `move.b $0016(a0),$0408(a5)` at
   `0x265c` has **12** xref sites: `jmp` from `0x7ac` (drain idle) and
   `0x816` (drain processed) plus 10 main-loop branch tails. The "no
   xref entry" reading came from the reference forms (`jmp`/`bra`, not
   `jsr`/`bsr`). Every measured drain leg stops **at** `0x265c`, so the
   C model has no `0x408` write for a different reason: the instruction
   is exactly one past the repo's measured boundary - live, unmeasured,
   not dead.
3. **"No writes exist ... `0x41c-0x421` ... `0x423-0x43F`" is wrong
   for `0x41c-0x428`.** ROM `0x2d2-0x2f2` writes seven master words via
   `movea.l a5,a0; lea $041c(a0),a0` + `move.w #imm,(a0)+`:
   `0x41c=0, 0x41e=0xc8, 0x420=0, 0x422=0x2c8, 0x424=0x88,
   0x426=0x48, 0x428=0` (`0x422` later overwritten by `0x40` at
   `0x300`). The post-increment form is invisible to a `(d16,a5)`-only
   scan. (Already modeled in the C boot mirror as `scsp_block[7]`.)
4. **`0x402` function identified**: `RBL = (v>>7)&3 = 2` -> 32K-word
   ring, `RBP = v&0x3f = 0x38` -> delay ring at word addresses
   `0x38000-0x3ffff` (bytes `0x70000-0x7ffff`). The constant keeps its
   measured value and now has a measured semantic.
5. **`0x405` confirmed** as MIBUF (MIDI-in) reads: boot drain
   `0x306-0x312` plus L3 `0x52a`. "Input, not DSP programming" stands;
   the C comment's "waste" wording is replaced by the MIBUF annotation.
