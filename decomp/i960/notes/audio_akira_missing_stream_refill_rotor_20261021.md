# Akira Ride the Tiger: rotor artifact traced to missing SCSP wave-page refills

Date: 2026-10-21. Verified on the owner's CRC-checked VF2.1 ROM
using the original Motorola 68000 stepper and the in-repository SCSP
software model. The ROM, PCM samples, diagnostics and WAVs are never
checked into GitHub.

## Actual PCM-memory evidence

The pitch/pan/DISDL corrections were sound, but the recognizable
ten-second Akira excerpt retained a conspicuous **helicopter-like**
rhythmic flutter. The original ROM's active slot 0 programs:

- SCSP slot register ctl=0x1831 (PCM8, forward loop), SA=0x10000;
- LSA=0, LEA=8191, PITCH=0x7800;
- only **512 of the first 8191 bytes** of SCSP-visible sound RAM
  at 0x10000 are nonzero at original musical tick 0; the remaining
  ~7679 entries in that read window are silent;
- SCSP slot 1 likewise has only 498 nonzero bytes, slot 2 has
  only 491, slot 3 has 508, slot 4 has 508. This is NOT evidence
  the original game intentionally plays thousands of zeros.

The sound CPU's *original driver* records reveal where the missing
pages are expected to come from. Slot 0's sound-RAM stream record
(0x2000 + slot*0x20) is active (flag 0x80) and retains:

- NEXT SOURCE: 0x8F96D4 in sample ROM (`samples + 0xF96D4`);
- NEXT DEST: 0x10200 in SCSP-visible sound RAM;
- after 882 experimental L2-only ticks, these pointers are
  **still unchanged**. Other melodic slots likewise retain
  next-page source and destination pointers.

The next 7680 bytes of that original ROM source are real nonzero
waveform data (7591 nonzero); they have not been consumed by the
test's original-MC68000 execution. The current original-ROM
music preview calls only L2 at 0x1F16 and B-class command drain
at 0x7A6, omitting the separate service path that refills
32 x 8KB/short wave windows. This omission is the best-supported
root cause of the pulse: playback reads ~512 bytes of wave, then
long zero-filled spans, then loops to the wave start.

## Experimental confirmation without falsifying the emulator

An explicitly named owner-only mode was added to
`tests/m68k/test_l3_real_rom_selections.c`: 

```sh
mkdir -p out
build/vf2_m68k_l3_selection_tests /local/vf2-roms \
  --akira-prefill-wav-dir out
```

It reads the *actual* source/destination pointers stored by
the original 68000 after notes are dispatched; while the stream
record remains flagged active it copies the **remaining pages of
that voice's 8192-byte wave window in advance** from the
checksum-validated sample ROM. Every copy is range-checked and
guarded by the actual record flag, valid source address, aligned
destination, sound-RAM window and unchanged-pointer check.
The experiment never runs for normal audio, public CI, or the
ordinary Akira `--akira-sequenced-wav-dir` output, and it is
NOT proof that the original 68000 performs those transfers
at those times.

Measured local original-ROM experiment:

| Metric | L2-only original-ROM preview | Prefill experiment |
| --- | ---: | ---: |
| Music commands dispatched | 456 | 456 |
| Refilled preview pages | 0 | 2025 |
| Non-silent stereo frames / 441000 | 415656 | 440988 |
| PCM peak (16-bit sample) | 8501 | 10893 |
| Relative envelope power in 6–11Hz band of 0.5–40Hz | 42.8% | 17.4% |

The last figure is a quantitative diagnostic of low-frequency
envelope pulsing; it does not directly prove subjective audio
quality, and a prefill can hide timing errors. We preserve the
original-ROM unmodified L2-only WAV as the baseline. The experimental
file is `vf2_akira_10s_PREFILL_UNVERIFIED_scsp_model.wav`, plus a
locally normalized *listening* copy with shared L/R gain.

## What the actual fix needs

1. Identify and execute the original 68000 sample page refill
   service (candidate is the main-loop stream/descriptor walker
   around 0x2242; not yet proven as the specific refill caller).
2. Measure per-voice copy trigger, DMA amount, source/destination
   cursor updates and loop-ring wrapping **against the genuine
   68000**. Produce a stop-PC original-byte differential.
3. Integrate that dispatcher into the correct scheduler alongside
   L1/L2/L3 and SCSP's 44100 Hz samples; do not hardcode
   page-prefill into production or shorten LEA artificially.
4. Compare resulting SCSP audio against a trusted external
   reference such as Mednafen's Sega Arcade SCSP Player:
   https://mednafen.github.io/documentation/sasplay.html
   and retain ROM-free regressions for the newly recovered logic.

Other modeled uncertainties remain: SCSP envelopes, DSP/wet output,
and exact timer cadence. **No claim of complete or board-identical
`Ride the Tiger` yet.**
