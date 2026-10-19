# v0898 — Original i960 ring writer → original vector-15 UART → original 68000 L3: measured 15-byte chain

Date: 2026-10-19. Repository: `celsowm/vf2-decomp` master.

## Scope and provenance

All ROM and snapshot data were obtained from the owner's original VF2.1
Model 2A chip archive, unpacked **locally only**. The native
`vf2i960 native-sixth-dispatch` GCC program built from the repo
generated a genuine ~11 MB i960 CPU/Model 2A machine checkpoint.
It reported `Native sixth-dispatch validation: MATCH` and full CPU
and memory parity. No ROM bytes, sound samples, snapshots, UART files
or WAVs were committed.

Starting from that checkpoint, an actual 600-frame run of
`vf2_host --oracle --fast` was executed twice:

- All-zero controller inputs: **0 original ring writes, 9 UART bytes**
  from 3 entries that were *already* pending, 0 KEYON, 0 modeled PCM.
- Deterministic controller input cycle
  `"0x100,0,0x80,0,0x10,0,0x20,0"`: **23 original ring
  writes, 30 modeled UART bytes**, 0 native D92 KEYON, 0 modeled
  nonzero PCM. The frame-level capture observed new queue writes
  at frames 325, 342 and 343.

These are *original i960 instruction executions* driven by
**synthetic ordinary controller inputs** (coin/start/punch/kick).
They are not artificially inserted audio queue commands.
The start snapshot has not been identified as active fighter combat.
**No naturally triggered fighter hit has been proved.**

## Frame 325: separating startup clears from the genuine writer

Original frame 325 (input `0x00`) has **17 aligned 32-bit
sound-ring bus writes**. The first is from original i960 PC
`0x00000100`, clears `0x00504020` to 0, and is not a
sound command. A later write from exact original PC
`0x00043924` stores `0x00AE101F` to `0x00504020`.

The first implementation of the independent oracle replay incorrectly
compared *only* the first ring write of the frame. This caused a
useful false-negative. The standalone
`vf2_audio_writer_replay` now checks for the expected
`(IP,slot,word)` among all original bus writes and retains the
first write separately for transparent diagnostics:

```text
original_i960_ring_writes=17
original_i960_first_writer_pc=0x00000100
original_i960_first_ring_word=0
matched_expected_writer=true
matched_write_index=16
MATCH original i960 writer PC / sound-ring slot / command word, write-index=16/17
```

The **owner-ROM replay exit code was 0**, from the filtered
original pre-frame checkpoint and original input `0`.
No downstream hit, hardware sound or IRQ scheduling is inferred.

## Frame 342: independently replayed original command and ISR

The SDL3 host's combined `--audio-pre-event-pc 0x43924` and
`--audio-pre-event-word 0x9e0a7f` filters saved the **original
pre-frame 342** CPU/machine state, not the earlier startup clear.
The saved checkpoint was then reopened in the **separate GCC
`vf2_audio_writer_replay` executable**, compiled in GitHub
Actions, and stepped through exactly one original i960 oracle frame
with ordinary controller input `0x20` (KICK).

Recorded and independently matched:

| Observation | Actual original ROM result |
| --- | --- |
| Oracle frame | 342 |
| Input mask | `0x20` |
| First original writer instruction IP | `0x00043924` |
| First original sound-ring address | `0x00504024` |
| First stored 32-bit word | `0x009E0A7F` |
| Original ring writes | **5** |
| Original queue count before/after | **0 → 5** |
| Replayed writer index | **0/5**, exit code **0** |

Disassembly of the owner's original ROM at this address:

```asm
00043924  92803903  st g0, 0x00504020[r3*4]
```

This is an **original i960 store** into the true sound-command ring,
rather than a guessed PC or a later receiver-side synthetic write.

The original `vf2_session_service_sound_drain` then entered and
serviced the **original ROM vector-15 handler** under the
emulated sound device's explicit IRQ request. It executed 16 ISR
services (including the final empty/disarm iteration), popped 5
original queue records, and emitted **15 actual ROM ISR UART bytes**:

```text
9e 0a 7f  cd 74 f8  ca 7e f8  cb 7f f8  ad 21 00
```

The same 15 bytes were independently recorded earlier by the
model's shadow-i960 UART bridge in the 600-frame host run.
The original-ISR comparison returned **exit code 0**:

```text
MATCH original i960 queue producer -> original ROM vector-15 ISR UART transcript (explicit IRQ service)
```

**Timing caveat:** the emulator explicitly serviced an external IRQ;
the test does not prove that the original Model 2A hardware would
schedule all ISR ticks in exactly the same frame or cycle.

## Original Motorola 68000 sound UART L3 differential — 15/15

The above exact ISR byte stream was submitted to the separately
compiled original-68000 stepper and recovered-C sound receiver with
the owner's original sound CPU ROM:

```sh
vf2_m68k_l3_selection_tests /local/owner/roms \
  --uart-stream 9e0a7fcd74f8ca7ef8cb7ff8ad2100
```

Both implementations agreed on the **full 512-KiB RAM and modeled
SCSP state after each of the 15 bytes**, with proper original
hardware-poke accounting (address `0x400001`). The original
68000 receiver filed **5** commands, ending with the ring write
cursor at `0x0014`. **Exit code 0**, exact output:

```text
MATCH C/68000 original UART stream bytes=15 ring_count=5 write_cursor=0014
```

Five isolated packets were also run, and each returned 0:

```text
9e0a7f  PASS
cd74f8  PASS
ca7ef8  PASS
cb7ff8  PASS
ad2100  PASS
```

**Important discovered framing correction:** original prefixes
`0xC0..0xDF` (here `CD`, `CA`, `CB`) file a
**two-byte ring record**, then `F8` is ignored. The earlier
`--l3-packet` test erroneously expected ALL ring files to
happen after byte three, causing three false test failures
*despite C-vs-original RAM parity*. This stale test contract
has been fixed. The `--uart-stream` option avoids artificial
packet boundaries entirely and differential-checks each byte.

## Reproduce without committing any proprietary asset

```sh
# Generate original CPU/Model 2A start state
vf2i960 native-sixth-dispatch /local/owner/roms out/sixth.vf2snap

# Capture the exact original frame-342 producer from ordinary inputs
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
vf2_host --rom-dir /local/owner/roms \
  --snapshot out/sixth.vf2snap \
  --oracle --fast --frames 600 \
  --input-cycle "0x100,0,0x80,0,0x10,0,0x20,0" \
  --audio-evidence-jsonl out/audio-frames.jsonl \
  --audio-pre-event-snapshot out/writer43924.vf2snap \
  --audio-pre-event-pc 0x43924 \
  --audio-pre-event-word 0x9e0a7f

# Independently step the ORIGINAL i960 frame and ORIGINAL vector-15
# ROM ISR. Expected transcript is observed, not guessed.
vf2_audio_writer_replay /local/owner/roms \
  out/writer43924.vf2snap \
  0x20 0x43924 0x504024 0x9e0a7f \
  9e0a7fcd74f8ca7ef8cb7ff8ad2100

# Differential the byte-for-byte UART transcript on ORIGINAL 68000
# and recovered C (no guessed 3-byte packet segmentation).
vf2_m68k_l3_selection_tests /local/owner/roms \
  --uart-stream 9e0a7fcd74f8ca7ef8cb7ff8ad2100
```

The ROM-backed CTest name
`vf2_m68k_original_frame342_uart_stream` runs only when
`VF2_ROM_DIR` is configured with an original verified ROM set;
ROM-free CI does not distribute proprietary assets.
`vf2_m68k_original_cx_short_command` pins the 2-byte framing.

## Still not proven

- Origin of the 5 commands in actual fighter-hit / collision code:
  **the KICK input bit alone is not causation**.
- Passive, cycle-exact vector-15 external IRQ timing on arcade silicon.
- Sound-board receive IRQ chronology in the same live hardware frame
  (the 68000 L3 comparison is a **replayed isolated** stream).
- A native gameplay scene producing nonzero SCSP PCM; this run had
  **0** D92 KEYON completions and **0** nonzero PCM frames.
- Hardware SCSP DSP, timer and DAC fidelity.

The next high-value recovery is to trace where `g0` at
`0x43924` was set, starting from the saved pre-frame 342 original
CPU state, and correlate the selector to actual fighter collision
instead of equating any game input with an audible impact.
