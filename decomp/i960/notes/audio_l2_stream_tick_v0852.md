# L2 stream tick recovered + L1/L2 per-frame wiring (v0852)

Status: slice closed. The `vf2_sound_board_maintain_streams` no-op
(v0796) is replaced by the measured L2 tick, and
`vf2_session_pump_audio` now advances the board one L1 + one L2
tick per pumped frame. Full suite green on the final tree;
committed with a ROM-free unit/regression layer plus the
C-vs-stepper differential legs in `tests/m68k/test_cdispatch.c`.

## 1. What was measured

The L2 timer ISR is `0x1f16..0x1f7a` (vector 26; the boot vector
table points there). Listing (exact bytes, `vf2m68k disasm`):

```
0x1f16  movem.l  $fff8,-(a7)        ; frame 0x7ecc..0x7eff (13 regs)
0x1f1a  move.w   #$ce,$0418(a5)     ; a5 = 0x100000 -> SCSP $418
0x1f20  move.w   #$40,$0422(a5)     ; SCSP $422
0x1f26  move.b   $1506(a6),d0       ; ring count byte (a6 = 0x1000)
0x1f2a  cmpi.b   #$ff,d0
0x1f2e  bcc      $1f76              ; count >= 0xff -> skip the body
0x1f32  lea      $2000(a6),a2       ; a2 = 0x3000
0x1f36  jsr      $21ba              ; prescaler (record 0 doubles)
0x1f3a  jsr      $1f7c              ; worker record 0 (0x3000)
0x1f3e  lea      $2020(a6),a2 / jsr ... eight workers, 0x20 stride
0x1f72  jsr      $1f7c              ; worker record 7 (0x30e0)
0x1f76  movem.l  (a7)+,$1fff
0x1f7a  rte
```

Worker `0x1f7c`:

```
0x1f7c  btst    #7,$0000(a2)       ; not armed -> rts
0x1f82  bne     $1f88
0x1f86  rts
0x1f88  subq.w  #1,$0002(a2)       ; word countdown, 16-bit wrap
0x1f8c  beq     $1f92              ; reached zero -> applier (below)
0x1f90  rts
0x1f92  ... applier: bclr bit0, stream bytes +1 latch, class jump
0x1fba  jmp     (a0)               ; 0x1fbc table (0x80/0xb0/0xc0-d0/0xf0)
```

Prescaler `0x21ba` (record 0's `[0xc]` rate / `[0xd]` countdown /
`[0xe]` beat value):

```
0x21ba  tst.b   $000c(a2)          ; rate 0 -> rts
0x21be  beq     $2240
0x21c2  subq.b  #1,$000d(a2)       ; countdown-- (8-bit wrap)
0x21c6  bne     $2240              ; nonzero -> rts
0x21ca  subq.b  #4,$000e(a2)       ; beat value -= 4
0x21ce  bmi     $2208              ; value < 4 -> stream-end body
0x21d2..0x2206   beat emit: be 10 <[$e]> (3 bytes) into the ring at
                 0x1000 + (ram[0x2504] & 0xfff), cursor += 4 (& 0xfff),
                 ram[0x2506] += 1, [$d] = [$c] reload, sr mask dance
0x2208..0x223c   stream-end: bclr bit7 of [$0], [$c] = 0,
                 be 50 7e 00 (4 bytes), cursor/count as above
```

Measured boot context (unchanged from F10, v0823): boot clears the
0x3000 records (boot master init), no ROM store outside this ISR
family arms them, so on every reachable state the prescaler exits at
its `[0xc] == 0` gate and the eight workers exit at their bit-7
gate: 51 instructions to the `rte`, zero RAM delta, the two SCSP
pokes the only state change.

## 2. What was recovered (C)

`src/hardware/sound_board.c` `vf2_sound_board_maintain_streams`:

- ring-count gate first (`ram[0x2506] == 0xff` skips the body);
- preflight refusal BEFORE any mutation: prescaler
  (`[0xc] != 0 && [0xd] == 1`) or an armed worker
  (`[0] bit7 && word[+2] == 1`) returns `VF2_ERROR_UNSUPPORTED`
  with `refused_stage` 1/2, `refused_record` (0..7), `timers_expired`
  = 1 and no state change (v0755a clean-refusal discipline: the tick
  as a whole did not happen, so nothing is half-applied);
- measured path only: prescaler `[0xd]--` (when `[0xc] != 0`),
  worker `word[+2]--` (when bit 7), both wrapping exactly;
- the two SCSP pokes (`$0418` = 0x00ce, `$0422` = 0x0040) applied on
  every non-refused tick (the head precedes the gate in ROM, so the
  gate-skip path keeps them);
- report: `descriptors_scanned` = 8 on a completed walk (0 on the
  gate path), `packets_emitted` 0 (the emit body is refused, never
  half-filed), cursor before/after reported.

The two live bodies the refusal names stay unmeasured on purpose:
the prescaler beat emit / stream-end (they write the L3 ring and
move the cursor/count) and the worker applier (reads stream bytes,
jumps the 0x1fbc class table). Both are the documented boundaries.

## 3. Host wiring

`vf2_session_pump_audio` (`src/app/session.c`) now, after the drain +
dispatch loop, runs exactly one L2 tick then one L1 tick
(`vf2_sound_board_maintain_voices`) per pump call, mirroring the
hardware's asynchronous ISR cadence at one tick per pumped frame.
The pump report gained `stream_ticks` / `voice_ticks` /
`stream_status` / `voice_status`; a refusal is reported without
failing the frame (`VF2_OK` return), the same convention
`drain_status` already used. This is the wiring item that
`audio_t5_closeout_v0825.md` left open: `vf2_game_update` keeps
refusing with an oracle attached (its guard is correct), so the
session pump is the live path, and `host_sdl.c` calls it once per
video frame before `vf2_session_render_audio`.

## 4. Differential evidence

`tests/m68k/test_cdispatch.c` `test_l2_stream_tick` — five
C-vs-stepper legs (full RAM below the ISR stack zone + at/above it,
full SCSP, a7 balance, zero bus events):

| leg | S path | S steps | C outcome |
|---|---|---|---|
| boot-idle, poke regs poisoned | full tick to `rte` | 51 | OK, 8 scanned, pokes restored 0x00ce/0x0040 |
| count = 0xff, armed record | gate skip | 7 | OK, 0 scanned, record untouched, pokes applied |
| countdowns live (5, 5, 0) | decrements, one wrap | 57 | OK, `[0xd]` 5→4, words 5→4 / 0→0xffff |
| worker word 1 | stops at `0x1fba` (word 0) | 32 | UNSUPPORTED, stage 2 rec 1, no mutation |
| prescaler `[0xd]` 1 | full tick, files beat | 67 | UNSUPPORTED, stage 1 rec 0, no mutation |

The stack-zone exclusion is `[0x7ec8, 0x7f00)` and that is a
measured widening of the triangle-L1 convention: besides the 52-byte
movem frame, the last `jsr 0x1f7c` (at 0x1f72) leaves its return
address `0x00001f76` at `0x7ec8`, so the residue reaches 4 bytes
below `0x7ecc`.

ROM-free layers: `tests/hardware/test_sound_board.c` pins the same
five shapes at the API level (including that the pokes land from
poisoned register bytes), and `tests/host/test_session_audio.c` pins
`stream_ticks == 1`, `voice_ticks == 1`, both statuses `VF2_OK` on
the T3 pump leg without disturbing its existing 9-bytes / 3-filed /
3-refused / 9-drain pins.

## 5. Supersessions

- `audio_sequence_pcm_v0796.md`'s "per-frame stream maintenance is a
  no-op" is superseded: the tick runs and is now recovered for its
  measured half.
- `audio_l2_tick_crash_v0812.md`'s crash verdict stays superseded by
  F10 (v0823): the full tick runs clean; the "crash" was a
  harness/entry artifact.
- `audio_roadmap_7of10_v0825.md` item 4 ("maintain_streams no-op may
  be wrong ... re-examined by S2") is resolved: the no-op rationale
  was wrong in the fail-closed sense, and armed records now refuse
  instead of silently accepting.
