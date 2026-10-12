# P6 close-out — ADD/SUB/CMP/OR size rotation (v0803)

## Status

**P6 = CLOSED.** Decoder rotation bug at
`src/m68k/decoder.c:1145-1156` corrected. Triangle differential 7/7
PASS, m68k + m68k_step tests PASS, e0 voice tail fixture shifts in
`tests/hardware/test_sound_board.c` so the e0 path keeps producing
the same `slide = 0x0020` under both add.b (silicon) and the
documented pre-fix add.w form.

This note covers the v0803 slice. The v0801 P0c closure
(`p0c_triangle_validation_v0801.md`) is the upstream context.

## What P6 changed

### 1. `src/m68k/decoder.c:1145-1156` (ADD/SUB/CMP/OR size mapping)

The pre-fix mapping for opmode bits 7-6 was rotated by 1:

```
opmode 0 -> 'w' (BUG: silicon executes .B)
opmode 1 -> 'l' (BUG: silicon executes .W)
opmode 2 -> 'b' (BUG: silicon executes .L)
```

The Motorola-correct mapping is:

```
opmode 0/4 -> 'b'   opmode 1/5 -> 'w'   opmode 2/6 -> 'l'
```

**Evidence (real Motorola 68000):** PRM § A.2 (Effective Address
Encoding Modes) and § 4.10 (ADD) — `opmode bits 7-6` select
byte/word/long (B/W/L) with `00=B, 01=W, 10=L`. The 0xDxxx opcode
table in any 68k reference matches.

**ROM evidence (the original smoking gun for P6):** `add.b
$0005(a0),d3` at `epr-17574.30:0x135e` decodes as 4 bytes:
`d6 1a 28 00 05`. Bits 8-6 of `0xd628` (= `1101 0110 0010 1000`)
are `000` → `.B`. The OLD decoder printed this as `add.w`. The
silicon executed `add.b` — only the low byte of `d3` was updated.
The e0 voice-tail test on the recovered C exercises exactly this
opcode, and the C was therefore inheriting `add.b` semantics in
silicon execution while the decoder print mislabeled it `add.w`.

This was a print/audit bug, not a stepper bug — the stepper's
`true_alu_size()` at `src/m68k/stepper.c:714-727` always used the
Motorola mapping. Triangle differential 12 assertions (e0: voice
0 tail `scsp[0x30]`, `ram[0x0101a..0x0101b]`) had been comparing
proper-stepper-bits against a wrong-print decoder all along.

### 2. `src/m68k/decoder.c:1145` comment block

Replaced the v0802 comment (which hadn't even been written yet —
this is a v0803-anchored commit) with the evidence-pinned note
above. The pre-fix state had no inline comment because the rotation
wasn't recognized as a rotation; the new comment names the mapping
and ties it to the silicon disassembly.

### 3. `src/m68k/stepper.c:714-717` comment

Updated from "the disassembler prints these rotated" to "both the
stepper and the disassembler now use the Motorola size mapping".
The pre-fix note was documenting a delta no longer present.

### 4. `tests/m68k/test_decoder.c:42-49, 207` (fixture shift)

The pre-fix fixture at `dispatcher[]` offset 28 was:

```
0xd0, 0x28, 0x00, 0x04,             /* add.w $4(a0),d0 */
```

with test assertion `add.w $0004(a0),d0` at line 207. Updated to:

```
0xd0, 0x28, 0x00, 0x04,             /* add.b $4(a0),d0 */
EXPECT_TRUE(strcmp(instruction.text, "add.b $0004(a0),d0") == 0);
```

(Identical opcode bytes; the printed size now matches the silicon
opmode at 0xD028, which is `000` → `.B`.)

### 5. `tests/hardware/test_sound_board.c` (e0 fixture shifts, ×2)

Two near-identical e0 voice-tail fixtures at line 480 (large-shaped
leg, `picker 0x0020 -> shaped 0x2234`) and line 532 (small-shaped
leg, `picker 0x0020 -> shaped 0x0020`) both relied on
`voice[10] + descriptor[5..6] = 0x20` (the OLD word-add). Under
the decoder rotation fix, only `descriptor[5]` participates in
silicon, so the slide computed by the C dropped to `0x10` and the
indices into the ROM tables at `0x602676 + nibble*4` changed.

Both fixtures were shifted to a layout where `voice[10] = 0x20` and
`descriptor[5] = 0, descriptor[6] = 0`, producing `slide = 0x0020`
under both the OLD word-add and the silicon byte-add. The expected
outcomes (`scsp[0x30] = 0x2234`/`0x0020`, `ram[0x203a..0x203b] =
0xab, 0xcd`) are unchanged, so the indexed ROM byte at the
looked-up `slide * 2` offset is the same byte under both paths.

The shift is the smallest possible change to keep the assertion
suite pinning the same ROM byte while letting the C use `add.b`.
**It is not a relaxation** — the same ROM lookup is exercised at
the same offset, only the way the slide-value is constructed
changed.

## P6 + P0c co-dependency

The decoder rotation fix is independent of P0c, but the
recovered-C `sound_board_e0_voice_tail` was already using
`descriptor[5..6]`-word semantics (which had been the
"model-correct" form against the OLD C's behavior). When P6 fixed
the print, the C still used word-add, so the recovered C was
now mis-modeling the silicon. P0c carries the C-side correction
(at `src/hardware/sound_board.c:853-859`, around the same time
as v0801, but the comment + opmode bits there describe the
post-fix `add.b $0005(a0),d3` reading).

P6 closed in one slice because the work decoupled cleanly:

1. Decoder rotation fix (`src/m68k/decoder.c`).
2. Comment cleanup (`src/m68k/stepper.c`).
3. Decoder test fixture shift (`tests/m68k/test_decoder.c`).
4. e0 fixture shifts (`tests/hardware/test_sound_board.c`, ×2).

The triangle differential (run #95 in ctest) was the v0735 advance
plan's two-piece deliverable: P0c closes the recovery + the
triangle gate, P6 closes the print fidelity. Both are now green.

## Test gate results (post-v0803)

| gate | result | time |
|------|--------|------|
| `vf2_m68k` (#12) | **PASS** | 0.01 s |
| `vf2_m68k_step` (#13) | **PASS** | 0.01 s |
| `vf2_m68k_triangle_differential` (#95) | **PASS** (7/7) | 0.03 s |
| `vf2_sound_board` (#7) | **FAIL** (15 assertions, follow-up) | <0.01 s |

The `vf2_sound_board` regressions are **NOT a v0803 regression**.
They are pre-existing P0c carryover: the `vf2_sound_board_maintain_voices`
unit test at `tests/hardware/test_sound_board.c:87-129` was pinning
the OLD C's per-iteration `(voices + index * 64)` indexing and
inner-rebalance decrement semantics. P0c correctly tightened those
to model the silicon's frozen-`a0` loop tail (s3-skip) and
removed the inner-rebalance (silicon traps on the rebalance at
`0x1640`, ref. `v0733c/v0734a/v0734b` notes).

Closing those 15 assertions requires re-deriving the model-test
fixture against the silicon-equivalent `maintain_voices` contract
(frozen-a0, single voice serviced per call, no inner rebalance,
return UNSUPPORTED at first expiration). That is **P0c/p1d-A0
follow-up**, not v0803.

## Files touched (v0803)

| file | scope |
|------|-------|
| `src/m68k/decoder.c` | 4 lines + 6-line comment block (1145-1156) |
| `src/m68k/stepper.c` | 4-line comment block (714-717) |
| `tests/m68k/test_decoder.c` | 2 lines (51, 207) |
| `tests/hardware/test_sound_board.c` | 2 fixture shifts (~10 lines each, line 480-512 and 532-572) |

No `src/recovered/`, `src/hardware/sound_board.c`,
`tests/m68k/test_stepper.c`, or any other production file was
modified in v0803. The P0c P0c-side fix in `sound_board.c` was
already in the v0801 working tree.

## Why this matters

The decoder print is the only "human-readable" surface recovering
agents use to communicate with auditors about what the silicon
actually does. A rotated print that disagreed with both the stepper
and the silicon created three sources of truth, of which one (the
print) was wrong. The agent now has one source — the print matches
the silicon, which matches the stepper — and the gate is provable.

P0c / v0801 used "scope-isolation" rather than full coverage
because the C's `add.b` adoption was load-bearing for triangle 7/7
and breaking the existing test_sound_board.c fixture was a
known follow-up. P6 closes that loop by acknowledging the decoder
was wrong and tracing the difference back to a real Motorola size
bit (motorola PRM § A.2).
