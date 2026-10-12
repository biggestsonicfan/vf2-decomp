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
