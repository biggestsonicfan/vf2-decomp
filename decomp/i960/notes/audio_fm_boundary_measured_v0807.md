# v0807 - FM boundary measured: driver never selects FM (sweep verdict)

Status: evidence note, no `src/` change. Settles the FM question with
ROM measurement instead of competing assertions.

## Retraction

The v0805 audit note's consequence 2 ("FM is live for music") is
**retracted**. It was sourced from a MAME issue comment, not from the
ROM — hearsay, exactly the class AGENTS.md forbids promoting. The
v0802 P5 note's "PCM-only" claim is **upheld**, but until now it was
also unmeasured (an assertion table with no ROM addresses). This note
supplies the measurement both were missing.

## Method

Full-image linear disassembly of the real sound program
(`vf2m68k disasm roms/vf2 0x0 260000`, 232,503 lines), exhaustive grep
for SCSP (`a5`-relative) writes, then per-site live/dead adjudication
by disassembling the surrounding window (phantoms in data regions
dismissed by incoherence: `ori.b #$1f,(a7)+` sleds, repeated
`or.l $00(a5,d0.w),d0`, `.word [unsupported]` interleaves).

FM select on YMF292 per MAME `scsp.cpp` (reference only): slot+`0x00`
bits 7-8 (SSCTL: 0 = PCM sample, 1 = noise, >= 2 = silence) and
slot+`0x0E` (MDL/MDXSL/MDYSL ring-modulation). Either nonzero means
"not PCM".

## Sweep result: no FM writer in real code

| Site | Form | Verdict |
|---|---|---|
| `0xb30`, `0xbce` | `move.w #$0,$0e(a5,d4.w)` | only `0x0E` writes in code; **zero** (PCM, dead voice-start chain) |
| `0x323dc` `or.l $0e(a5,d0.w),d7` | data-region phantom | dismissed (0x32xxx is tables) |
| `0xc8e` / `0xda8` / `0xee4` KEYON | `move.l d6/d5,$00` = `0x10300000\|SA20` | SSCTL = 0 (PCM; dead/twin chains) |
| `0xd7e` / `0xefa` / `0xf02` KEYON RMW | `ori.b #$18` on slot+0 high byte | sets KEYONB+KEYONEX only, preserves rest (twin, real) |
| primer `move.b #$10,$00` (`0x964/0x9f4/0xa96/0xf74/0xfd6/0x11f2/0x1206`) | high byte `0x10` | SSCTL = 0 (PCM) |
| `0x24dc` / `0x2616` walk RMW | `andi.b #$9f` on slot+1 | clears LPCTL, **preserves** SSCTL bit 7 (live walk, F5-swept) |
| `0x162c` B0 shaping | slot+`0x13` RMW (LFO byte) | PCM-path vibrato/tremolo, not FM modulation |
| `0x69a4` `ori.l #$ef060000` (SSCTL=2!) | `0x6990` sled/data region | phantom, dismissed by window incoherence |
| `0x75a6`, `0x8832`, `0x12xxx`-`0x34xxx` bset/bchg/or.l | sled/data regions | phantoms, dismissed |

No real-code site writes nonzero `0x0E`, and no real-code site sets
SSCTL. The driver is PCM-only by exhaustive measurement.

## Attempted guard and why it was reverted

A nonzero-`0x0E`/`0x0F` refusal was implemented in
`vf2_scsp_write_u8/u16`, proven able to fail (3 refusal + 1
transactional assert failed with the guard removed), then **reverted**
because it broke green `vf2_sound_board` legs. Root cause, worth
preserving:

- The driver strides SCSP by raw slot *word* (`SCSP[slot+0x0b]`),
  while the model partitions by `0x20` slots. The unit fixture uses
  slot words `0x0003/0x0004` (synthetic, small), so its live V1
  key-off write lands on SCSP `0x000e` — which the model reads as
  "slot 0 + `0x0E`" (FM word). Real ROM slot words are `index*0x20`
  (boot `0x3a8` table), where `+0x0b` can never alias `+0x0e`.
- A bus-level FM guard therefore misfires on a committed green test.
  No *live* path can produce nonzero `0x0E` (sweep), so the guard buys
  nothing reachable and costs a green gate. Reverted; `src/` and tests
  byte-identical to head.
- Follow-up (not this slice): the `0x0003/0x0004` fixture slot words
  are ROM-unrealistic; realigning them to `0x20`-stride values would
  remove the alias class, but churns ~15 asserts for no live-path
  gain. Deferred.

## What the synthesis gap actually is (corrected)

Not FM. The PCM renderer is correct-by-measurement for every
driver-reachable state. The real gaps:

1. Live voice-START (KEYON) path still refused (staged-silence
   terminal) — the recovery frontier, unchanged.
2. Envelope/pitch/loop tables in `scsp.c` are spec-written (MAME-shaped),
   never differential-proven: no live driver path KEYONs, so no
   differential exercises them. They are navigation-grade until a live
   KEYON exists.
3. KEYONEX side effect (global KEYON scan on slot+0 bit-12 writes) is
   unmodeled — confined to dead/post-refusal code on all measured
   paths (no live writer sets it), so currently harmless. Recorded,
   not fixed.
