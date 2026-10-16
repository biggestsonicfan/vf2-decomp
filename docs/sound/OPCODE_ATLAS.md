# Audio opcode atlas — Virtua Fighter 2.1 (Model 2A)

> Base: dc7b856b9d70. This is a **source-indexed audit**, not
> a dump of the copyrighted 68000 ROM. Hex opcode bytes below come from
> existing ROM-free decoder fixtures; a ROM-PC attribution is made only
> where a project's ROM analysis note explicitly states it.
> For exact per-PC bytes run `vf2m68k disasm <roms> <pc> <n> --raw` locally.

## Binary and CPU contract

The sound processor is Motorola 68000. Its program is reconstructed in
big-endian instruction-byte order as the `audiocpu` region by `vf2rom`
from the sound EPR. Each instruction consists of a 16-bit opcode and
zero or more extension words. The public struct
[vf2_m68k_instruction](../../include/vf2/m68k.h) includes opcode,
instruction length, decoded text and decoder support status. It is
**not** a complete 68000 legality oracle. Unsupported must stay
unsupported until supported by precise measurement.

- `.b` on Dn observes/changes only 8 low bits; `.w` only 16 low
  bits; `.l` 32 bits. A `cmp.b` does not read the high bits of Dn.
- PC-relative forms (Bcc.w, DBcc, JSR/JMP, PC-addressing forms)
  use **opcode PC + 2** at the extension word. This is pinned in
  [test_pcrel_base.c](../../tests/m68k/test_pcrel_base.c).
  The previous +4 bug made valid branches appear to land inside
  opcodes; see [re-audit](../../decomp/i960/notes/audio_pcrel_base_reaudit.md).
- With A6 = `0x1000`, operand `$1508(a6)` maps to sound RAM
  **`0x2508`**, not `0x1508`.
- For indexed EA `(a5,d4.w)`, the full sign-extended index word
  matters; never mask to a slot unless ROM instructions do so.

## Opcode-word / extension-word examples

These encodings are **test fixtures**, not claimed ROM bytes at
specific PCs. Source: [test_decoder.c](../../tests/m68k/test_decoder.c).

| Words, BE hex | Decoded instruction | Relevant property |
|---|---|---|
| `70fe` | `moveq #-2,d0` | signed 8-bit to 32-bit |
| `0280 0000 0fff` | `andi.l #$fff,d0` | clears upper bits |
| `1d40 150e` | `move.b d0,$150e(a6)` | one RAM byte |
| `586e 1508` | `addq.w #4,$1508(a6)` | ring cursor, 16 bits |
| `026e 0fff 1508` | `andi.w #$fff,$1508(a6)` | modulo 0x1000 |
| `0828 0000 0002` | `btst #0,$2(a0)` | tests a flag bit |
| `b412` | `cmp.b (a2),d2` | compares **byte**, not word |
| `43fb 2006` | `lea $6(pc,d2.w),a1` | PC-relative indexed EA |
| `4eba fff8` | `jsr (d16,PC)` | PC+2 extension base |
| `0802 0008` | `btst #8,d2` | bit 8 of data register |
| `082d 0007 0409` | `btst #7,$0409(a5)` | peripheral status |
| `48d2 00ff` | `.word $48d2` | illegal reg→mem MOVEM EA |
| `1508 0280` | `.word $1508` | illegal MOVE.B An source |
| `4e75` / `4e73` | `rts` / `rte` | subroutine / interrupt exit |

ROM-measured cross-links: `B412` at `0x66e` and `B42A` at
`0x6c0` are byte-size comparisons
([V1](../../decomp/i960/notes/audio_v1_voice_isr_v0821.md)).
An invalid `48D2` MOVEM appears in the engine copy-loop
at `0xcb6` ([E1a](../../decomp/i960/notes/audio_e1a_engine_map_v0825.md)).
The apparent `0x7bc` byte MOVE from An is an illegal-EA form;
the correct dispatcher head is at `0x7a6`.

## 68000 instruction-PC atlas

These PCs are **sound program offsets**, not `0x600000 + PC`
bus addresses. Intervals may span multiple instructions.

| PC | Instruction or flow | Measured effect / source |
|---|---|---|
| `0x28e` / `0x2b2` | `move`/`lea` master init and loop | SCSP 32 x 0x20 slots; [silicon audit](../../decomp/i960/notes/audio_silicon_audit_scsp_confirmed_v0805.md) |
| `0x49e` | rotate byte, `ori.b`, write `$400001` | staging dance [8c](../../decomp/i960/notes/audio_8c_comms_hunt_v0825.md) |
| `0x526` | L3 UART ISR, byte read and ring assembly | [8g](../../decomp/i960/notes/audio_8g_l3file_v0825.md) |
| `0x5fe` / `0x61a` | L1 voice `btst`/`subq.b`/`dbf` scan | [V1](../../decomp/i960/notes/audio_v1_voice_isr_v0821.md) |
| `0x636` / `0x684` | key-off control stores; decrement counters | [V1](../../decomp/i960/notes/audio_v1_voice_isr_v0821.md) |
| `0x7a6` / `0x804` | dispatcher / `addq.w #4,$1508(a6)` | [drain](../../decomp/i960/notes/audio_drain_path_correction_v0794.md) |
| `0x902..0x95a` | Loop-B, 32-voice scan and branch | [S2d](../../decomp/i960/notes/audio_s2d_loopb_v0825.md) |
| `0xa8c..0xc1e` | `mulu.w`/shift/`not.b`, slot authoring | [S3](../../decomp/i960/notes/audio_s3ab_setup_v0825.md) |
| `0xc68` | `move.l`/`btst #31` -> key-on selection | [E1a](../../decomp/i960/notes/audio_e1a_engine_map_v0825.md) |
| `0xcb6` | illegal `48d2` | fail closed; [E1a](../../decomp/i960/notes/audio_e1a_engine_map_v0825.md) |
| `0xd92..0xdf8` | alternative engine stores, invalid-copy exit | [E1i](../../decomp/i960/notes/audio_e1i_d92_v0825.md) |
| `0xed8..0xf10` | exact KEYON instruction chain below | [E1c](../../decomp/i960/notes/audio_e1c_recovered_v0825.md) |
| `0xf12` / `0xf50` | allocator / exhausted reaper | reaper open [E1o](../../decomp/i960/notes/audio_e1o_loopb_wiring_v0825.md) |
| `0x11f8` | `bset #0,voice+2` | arms L1 [E1f](../../decomp/i960/notes/audio_e1fa_80row_v0825.md) |
| `0x1f16` / `0x1f7c` | L2 tick / stream interpreter | [8c](../../decomp/i960/notes/audio_8c_comms_hunt_v0825.md) |

### Key-on: instruction → register → hardware write

The ROM-measured, **synthetic-input** E1c differential takes the
table+0 bit31 branch; it is not a gameplay-trigger observation.

| Opcode-PC | Instruction(s) | Architectural effect |
|---|---|---|
| `0xed8` | `andi.l #$fffff,d5`; `ori.l #$10300000,d5` | composes address/control |
| `0xee4` | `move.l d5,$00(a5,d4.w)` | slot+0 SCSP long |
| `0xee8` | `move.l $08(a2,d3.w),d5`; `move.w d5,$04(a5,d4.w)` | SCSP slot+4 |
| `0xef0` | `move.l $04(a2,d3.w),d5`; `subq.w #1,d5`; `move.w d5,$06(a5,d4.w)` | low 16-bit sub, slot+6 |
| `0xefa` | `move.b slot+0,d5`; `ori.b #$18,d5`; `move.b d5,slot+0` | RMW KEYONB/KEYONEX |
| `0xf06` | `lea $1000(a6),a2`; `move.b #$40,$00(a2,d4.w)` | RAM `0x2000+slot` |
| `0xf10` | `rts` | return to `0x7f6` |

Source: [E1c recipe](../../decomp/i960/notes/audio_e1c_keyon_recipe_v0825.md)
and [E1c differential](../../decomp/i960/notes/audio_e1c_recovered_v0825.md).
Previously measured: 381 steps to `0xf10`, full RAM + SCSP
match under stated exclusions, slot active, render peak 4251.
These are **existing fixture results**, not newly executed tests.

## i960 instruction-level audio path (no invented raw opcode words)

| PC i960 | Purpose |
|---|---|
| `0x437d0` | UART setup and IRQ-10 ack |
| `0x43800` | queue drainer: empty path `ldib`/`cmpible`/`ldob`/`cmpobe` (four instructions) |
| `0x43868` / `0x43884` | UART store PC / unstepped return |
| `0x43888` / `0x438fc` | filer / ring write |
| `0x43abc` / `0x58eac` | boot flusher / filer caller |

Source: [drainer test](../../tests/recovered/test_sound_43800_native.c)
and [8f](../../decomp/i960/notes/audio_8f_drainer_v0825.md).
The printed `cmpible` condition did **not** explain the
measured byte-index rule: trust differential states over
the mnemonic alone. To obtain hex opcode words on i960,
run local `vf2i960 disasm` / `vf2probe --trace`;
none are fabricated here.

**Do not confuse:** `UNSUPPORTED` by the stepper,
an illegal silicon opcode, an unmodeled SCSP write, and
unreachable code. All need distinct evidence and tests.

Next: [OPCODE_TRACE.md](OPCODE_TRACE.md),
[REVERSE_ENGINEERING.md](REVERSE_ENGINEERING.md).
