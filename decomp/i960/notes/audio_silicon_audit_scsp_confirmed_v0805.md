# v0805 - Audio silicon audit: SCSP-family confirmed, MultiPCM rejected

Status: evidence note, no `src/` change. Resolves the online-docs question
from the sound plan: which public manual describes the VF2 2.1 sound chip.

## Question

General Model 2 wiki pages claim `2x Sega 315-5560 MultiPCM` (Yamaha
YMW258-F, port-based: slot-select + reg-number + data ports, 28 voices,
11 byte-regs per voice, 12-byte sample headers in sample ROM).
The repo models a memory-mapped Saturn-shaped SCSP (`src/hardware/scsp.c`:
32 slots, `0x20` stride, `0x1000`-byte window at 68k `0x100000`, master
block `0x0400`, wave area `0x0700`). Both cannot be right for this program.

## Method

ROM disassembly of the real sound program via the repo tool
(`build/Debug/vf2m68k.exe disasm roms/vf2 <addr> <count>`), plus a MAME
cross-check. No wiki label is treated as evidence.

## ROM evidence (all memory-mapped SCSP-family)

| Site | Disasm | Shape |
|---|---|---|
| boot `0x28e` | `move.b #0,$0401(a5)` / `lea $0700(a5),a0` wave copy / `move.w #$138,$0402(a5)` | master `0x0400` block + `0x0700` wave upload |
| boot slot loop `0x2b2` | `move.b #$ff,$000b(a0)` / `move.b #$ff,$000d(a0)` / `move.b #16,$0000(a0)` / `lea $0020(a0),a0`, `d7=0x1f` (32 iters) | `0x20` stride, byte offsets `0x00/0x0b/0x0d` |
| master tail `0x2f4` | `move.w #$ce,$0418(a5)` / `#$3c0,$041a(a5)` / `#$40,$0422(a5)` | `0x0400` timer/control words |
| key-off `0x636/0x684` | `d1=voice[0]` slot word, `move.b #$1f,$0b(a5,d1.w)`, `ram[0x2000+d1]=0` | indexed SCSP + RAM stride, no masking |
| voice-start prefix `0xa8e-0xc44` | `move.b #$10,$00(a5,d4.w)` / `move.b d6,$0d(a5,d4.w)` / `move.l #$1fff,$04(a5,d4.w)` / `move.b #0,$0c` / `move.w #0,$0e` / `move.b ..,$15` / pan `move.b d6,$16(a5,d4.w)` | slot+`0x00/0x04/0x0c/0x0d/0x0e/0x15/0x16` word/byte sas |
| sample stage `0xc2a` | `lea $5000,a2` / `move.l $10(a2,d3.w),$08(a5,d4.w)` | slot+`0x08` = 32-bit staged sample address from `$5000` table |

A port-based MultiPCM (`write slot-select / write reg-number / write
data`, sample *index byte* to reg 1, env/LFO auto-load from 12-byte
sample-ROM headers) has no `0x20`-stride window, no slot+`0x08` address
long, no `0x0400` master block, no `0x0700` wave area. None of the six
sites above can be produced by that interface. **MultiPCM is rejected
for this program.** Whatever other Model 2 boards carry, the VF2 2.1
sound driver as shipped drives a memory-mapped SCSP-family chip through
`a5 = 0x100000`.

## MAME cross-check

MAME `issue #14315` discusses Model 2 audio through **the SCSP** and
notes *"Virtua Fighter 2 uses FM synthesis for music"* (most Model 2
games use PCM exclusively). Two consequences:

1. MAME agrees with the ROM: SCSP, not MultiPCM, for this game.
2. **FM is live for music** — the repo's PCM-only renderer
   (`vf2_scsp_render`) plus spec-written envelope/pitch/loop semantics
   is missing the music path, not a hypothetical. FM boundary
   measurement is the real synthesis gap (not DSP: v0804 already closed
   the `0x400-0x43F` write set at 7 addresses with no DSP-window
   traffic).

## Online-docs ranking for future slices

1. ROM driver (oracle, via `vf2m68k` + stepper events).
2. Saturn SCSP User's Manual (`ST-077-R2-052594`) — suggestion only.
   Register names/offsets must each be confirmed against a driver write
   before entering `src/` (cf. v0799b `SCSP+0x4000` retraction).
3. MAME Saturn-SCSP implementation — second suggestion / cross-check.
4. Motorola `M68000 Programmer's Reference` — stays authoritative for
   `src/m68k/stepper.c` instruction semantics only.
5. MultiPCM docs (Furnace `multipcm.md`, MAME `multipcm.cpp`,
   `YMF278B/OPL4` manual, `315-5560` wiki claims) — **rejected** for
   this program. Do not port reg `0-10`, `VALUE_TO_CHANNEL`, or 12-byte
   header handling. Do not rename `scsp.c` to `multipcm.c`.

Per `docs/LEGAL.md`: manuals are read-only measurement guides. No PDF
text, no MAME code, no ROM bytes committed — this note cites addresses
and instruction shapes only.

## Consequences

- Keep the `scsp.c` family and the `32 x 0x20` window: ROM-measured.
- Keep envelope/pitch/loop/FM/DSP marked spec-written or open: the live
  driver path (drain-driven staged silence, v0804) never KEYONs, so the
  renderer semantics have no ROM-proven caller yet.
- Next: Slice B minimal 68k oracle, then FM boundary measurement
  (which driver writes select FM vs PCM per slot, and what the music
  command path stages before them).

## Correction (v0826)

- Lines 13/25/35 ("wave area `0x0700`", "`0x0700` wave copy/upload"):
  wrong name. `0x0700` is the SCSP **DSP register surface** (COEF
  `0x700-0x77f`, MADRS `0x780-0x7bf` + mirror `0x7c0-0x7ff`, MPRO
  `0x800-0xbff` = 128 steps x 4 words), and the boot's `0x500`-byte copy
  into it (ROM `0x28e`) is a DSP **effect-program upload**, not wave
  data. The `0x05xx` sample staging is the wave area.
- Lines 51-52 ("v0804 already closed ... with no DSP-window traffic"):
  superseded - v0804's scope (`0x400-0x43f`) could not see the window.
  See `audio_dsp_program_measured_v0826.md`.
- Line 76 ("keep ... FM/DSP marked spec-written or open"): the DSP
  configuration half is now ROM-measured (program text, ring config,
  effect-return arming). DSP *execution* and audible output remain
  external-silicon/unmodeled.
