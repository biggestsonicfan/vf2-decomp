# v0788 - audio B0 table surface measured; d0-provenance question stated; re-entry cap closed

Status record, not a fix. `src/` byte-identical.

## 1. B0 dispatch is a pure 128-entry jump table (sole router)

Head at audio-CPU 0x13ca (file offsets == tool-relative addresses):

```
0x13ca: move.b (a3)+,d0     ; d0 = first byte, a3++
0x13cc: move.b d0,d2        ; d2 = same
0x13ce: move.b (a3),d0      ; d0 = OVERWRITTEN with next byte (peek)
0x13d0: andi.w #$7f,d2      ; d2 = entry
0x13d4: lsl.w #2,d2
0x13d6: lea $06(pc,d2.w),a1 ; a1 = 0x13de + entry*4 (PC points at ext word)
0x13da: jmp (a1)
0x13dc: rts                 ; default: ignore
```

Table base 0x13de, stride 4 (`jmp (xxx).W`). Full dump: 110 of 128
entries → 0x13dc. The 18 live entries (entry → target):

| entry | target | C-model status |
|---|---|---|
| 0x01 | 0x15de | recovered (entry 1) |
| 0x02 | 0x164c | recovered (entry 2) |
| 0x07 | 0x16ba | recovered (entry 7) |
| 0x0a | 0x174c | recovered (entry 10) |
| 0x10 | 0x181c | recovered (entry 16) |
| 0x12 | 0x18a8 | REFUSED — tiny: `lea $2000(a6),a0; move.b d0,12(a0);
  move.b d0,13(a0); rts` (payload bytes to absolute 0x300C/0x300D) |
| 0x18 | 0x18b6 | REFUSED — medium (~25 instr): 0xA0-byte copy from
  table at absolute 0x603066+((d0&0xf)*4) to 0x3000(a6), then a
  10-iter volume-scale loop over command descriptors |
| 0x29 | 0x1912 | recovered (entry 0x29) |
| 0x2a | 0x194a | recovered (entry 0x2a) |
| 0x2b | 0x1986 | recovered (entry 0x2b) |
| 0x2c | 0x19c2 | REFUSED — complex (~40+ instr): saves 32
  descriptor[13] bytes to 0x3250(a6), sample-table copy via table
  at absolute 0x60493e indexed by d0&0x7f, then restores |
| 0x2d | 0x1a48 | REFUSED — small (~12 instr): threshold test against
  table at absolute 0x605368, 16-byte stride copy to 0x17(a5+d4);
  falls through to 0x1a76 on miss |
| 0x2e | 0x1a76 | REFUSED — small (~12 instr): channel from RAM
  0x150e, combine chanstruct with ((d0&0x7f)<<1)&0xe0, store to
  0x1017(a5) |
| 0x2f | 0x1a9a | REFUSED — small (~11 instr): channel from RAM
  0x150e, combine chanstruct&0xe0 with the 128-byte table at
  0x179c (== `sound_board_b0_entry_10_table`), store 0x1017(a5) |
| 0x3a | 0x1ac0 | REFUSED — not yet disassembled past entry point |
| 0x40 | 0x1b4c | recovered (entry-0x40 explicit path) |
| 0x50 | 0x1c0a | STRUCTURAL MISMATCH, see section 2 |
| 0x7b | 0x1de0 | shared with C0 (`jsr 0x1de0` at 0x1e6e); the C
  `sound_board_clear_c0_voices` covers its key-off/counter core
  but drops the $400b/$4000(a5) + $4000(a3-stream-desc) stores |

No static jsr/jmp/bra/bsr/bsr.w or long-pointer reference to ANY
of these handler addresses exists anywhere in the 0x80000-byte
sound program (exhaustive scan). The table's `jmp (a1)` is the
sole router (plus C0's direct `jsr 0x1de0`).

## 2. The d0-provenance question (one question, two symptoms)

Handler entry state: d2 = payload&0x7f (table index, consumed),
d0 = the byte AFTER it in the a3 stream (9/9 handler-address
matches prove the d2 half; the d0 half is the question).
Two independent handlers (0x16ba entry 7, 0x1912 entry 0x29)
match the C model's `payload`-formulas bit-exact — but that
proves formula SHAPE, not the source byte. The C tests are
synthetic and never run ROM code, so they cannot validate it.

Symptom A (0x50 vs 0x7d/0x7e/0x7f): the table routes entry 0x50
→ 0x1c0a and entries 0x7d/0x7e/0x7f → rts. But 0x1c0a itself
opens with `cmpi.b #$7d,d0; beq 0x1cce / cmpi.b #$7e,d0; beq
0x1d60 / cmpi.b #$7f,d0; beq 0x1c26 / rts`. If d0 == payload,
reaching 0x1c0a requires payload ∈ {0x50, 0xD0}, the three
guards can never fire, entry 0x50 is a NO-OP, entries
0x7d/0x7e/0x7f are NO-OPs — and the C model's elaborate
key-off-all + counter-decrement legs for entries
0x7d/0x7e/0x7f would be PHANTOM (ROM-unreachable via the sole
router). If d0 == next-queue-byte, the legs are live end-of-
sequence handling (0x7d/0x7e/0x7f = class-0x70 marker bytes)
and the C model's `entry` variable is really the LOOKAHEAD
byte, not payload&0x7f. Both readings cannot be checked
without the class-drain's a3 setup (still unfound: no caller
of 0x13ca by any static reference kind).

Symptom B (a5 window): entries 0x2e/0x2f and the voice-loop
tails of entries 1/2/10 traffic in `$13/$16(a5+slot)` and
`$4013/$4016/$1017(a5)`. The C model abstracts the slot-based
ones as SCSP-register writes (`sound_board_update_voice_
packed_register`: slot*0x20+0x13) while ROM uses the slot
word UNSCALED as a byte offset — the "wider register window"
the code comments already flag as unmodeled. a5's identity
(SCSP base? RAM mirror?) is undetermined; its init (`lea
,a5` at boot) was not located in this slice.

DO NOT recover entries 0x12/0x18/0x2c-0x2f/0x3a/0x50/0x7b, and
DO NOT "fix" the 0x7d/0x7e/0x7f legs, until d0-provenance is
settled by finding the class drain (what sets a3 before the
B0 head) or by a 68000 executor oracle. Easiest next step:
locate the queue drain — it must read count/cursor RAM and
call per-class heads; the 0x1330 loop (jsr 0x1356) is only the
per-descriptor SCSP applier, not the drain.

## 3. Re-entry cap CLOSED (verified guard, stays)

ROM 0x2188-0x218e and 0x219c-0x21a4 both `bra $1f98` back to
the stream decoder with NO counter — a cyclic chain loops
forever on hardware; shipped data is acyclic. The C model's
`pointer_reentries > 8 → VF2_ERROR_UNSUPPORTED` is therefore
a host-side hang guard, not ROM behavior: correctly
fail-closed, correctly kept. Fidelity spot-checks pass: F2
sentinel clears 64 longwords at absolute 0x2000
(`lea $2000(a6),a0; moveq #63,d4; clr.l (a0)+; dbf` =
sound_ram 0x3000, 64 × 4 B ✓ matches the C comment).

## 4. Class data tables exist for D0/F0 (open, needs the drain)

`lea $60c726,a2` table (file 0xc726, absolute addresses):
0x80→0x60c766, 0x90→0x60c8e6, 0xa0→0x60ca66, 0xb0→0x60cbe6,
0xc0→0x60cd66, **0xd0→0x60cee6**, 0xe0→0x60d066,
0xf0→0x60d1e6. The C `dispatch_next` refuses classes other
than 80/90/a0/b0/c0/e0. Whether D0/F0 commands are live
needs the drain, same dependency as section 2.

## 5. C0 head verified single-fetch (contrast for section 2)

0x1e5a: `move.b (a3)+,d0` then uses d0 as payload directly
(descriptor[2] = payload, `>= 0x70` → `|= 0x80`, `jsr 0x1de0`)
— matches the C c0 path exactly with no peek. The B0 head's
peek-then-dispatch shape is unique to the table path.

## File map

- `src/hardware/sound_board.c`, `include/vf2/sound_board.h`:
  UNCHANGED (boundary preserved).
- Evidence: `vf2m68k disasm` over 0x13ca/0x13de-table/
  0x16ba/0x174c/0x1912/0x18a8/0x18b6/0x19c2/0x1a48/0x1a76/
  0x1a9a/0x1c0a/0x1de0/0x1e5a/0x2160 + raw-word verification
  (`epr-17574.30`, 16-bit word-swap) + exhaustive
  jsr/jmp/branch/pointer scans (all empty).
