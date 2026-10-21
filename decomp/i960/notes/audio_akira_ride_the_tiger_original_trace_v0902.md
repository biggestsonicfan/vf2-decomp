# Akira's Ride the Tiger — original-ROM sound-test / 68000 trace

## Source of the identification

On the verified VF2.1 Model 2A arcade owner ROM, the original i960
SOUND TEST selector (see `selector17_index2_sound_test.md`) contains
297 numbered entries beginning at maincpu ROM offset `0x76b48`.
Entry **#11** at `0x76be0` is explicitly named **Akira** and its
little-endian queue command is **`0x00ad1004`**, sent over L3 as
`AD 10 04`. This is the character's track known as *Ride the Tiger*.
The earlier frame-342 `9E 0A 7F` WAV is **SOUND TEST #34 coin**;
it is not Akira's theme or a proven punch sound.

## Exact original sound CPU source data

The SOUND CPU original EPR uses `LOAD16_WORD_SWAP` on `epr-17574.30`.
The A0 lookup selects ROM row `0x608f24` (index `0x10`), then
record `0x609c36` (argument `0x04`). Its header is
`10 10 10 10`; its six stream roots are:

| Root # | Original sound CPU bus pointer |
| --- | --- |
| 1 | `0x615773` |
| 2 | `0x615ad7` |
| 3 | `0x616009` |
| 4 | `0x616b77` |
| 5 | `0x6174ac` |
| 6 | `0x617e0c` |

The corresponding ROM table is independently checked by
`tools/python/audio_sound_test_catalog.py`; the ROM-free and
owner-opt-in regression is `test_audio_sound_test_catalog.py`.
No ROM, music-bank, WAV, or game-original sound samples are
embedded in the repository.

## Measured playback milestone

The owner-ROM original 68000 L3 receiver agrees exactly with native C
for the isolated `AD 10 04` packet. The original 68000 dispatcher
enters A0 at `0x1e98`, completes its low-header music branch
and installs the first music stream at `0x615773` into descriptor 0.
Native C **currently refuses at `0x1e98`**, so the recovered C
music sequencer has **not** reached parity. This is crucial: a
successful original-only probe does not establish native playback.

An early experiment cleared all 20 original boot channel descriptors
and created a synthetic channel-D descriptor. It drained 32 music
commands with zero KEYONs — **a false negative caused by the fixture**.
The subsequent original-ROM probe preserved the exact untouched
20-descriptor boot image and obtained:

- Original L2 at `0x1f16` executed **1,073 instructions** and filed
  **32 music commands** on its first tick.
- 32/32 commands then drained in the original Motorola 68000
  (no unsupported opcode / no failed return).
- Original music packets included `9F`, `90`, `91` and `96`
  note/voice commands; **8 modeled SCSP voices** became active.
- A private opt-in WAV generated from this original-authored
  SCSP state via `vf2_scsp_render` contains **88,200 stereo frames**
  (2.0 seconds at 44,100 Hz), **88,133 non-silent** frames,
  with peak absolute signed-PCM **32,768** (some clipping).
- Local WAV SHA-256:
  `e91a8329cf92f7894f0674d09f9f3dc537acdf4bf0a5b8a0dd17e54cc0598ff4`.

Explicit local original-ROM command:

```sh
mkdir -p out
build/vf2_m68k_l3_selection_tests /path/to/unpacked/vf2 \
  --akira-wav-dir out
```

The default test never writes WAV. A separate strict regression
`--verify-akira-music` requires original L3, 32 commands drained
after the first L2 tick, eight active voices and nonzero model PCM;
enable through CMake `-DVF2_VERIFY_AKIRA_MUSIC=ON` and
`-DVF2_ROM_DIR=/path/to/unpacked/vf2`.

## Stereo distortion diagnosis and direct mixer correction (October 21, 2026)

The first original-ROM generated ten-second WAV was audibly recognizable
but distorted and almost mono. Direct PCM inspection established:
- left: 31,548 clipped samples (7.1537% of the left channel);
- right: 22 clipped samples; 82.65% of right-channel frames zero;
- severe left/right RMS disparity (15,476 / 2,961).

The source cause was NOT incorrect original song identity or the
`AD 10 04` event. The SCSP software renderer used a fictional
**linear** pan (DIPAN 0=full left, 31=full right) and **ignored DISDL**.
That contradicts the original SEGA *SCSP User's Manual*, §4.2.7 MIXER,
https://www.infochunk.com/saturn/segahtml_en/hard/scsp/hon/p04_27.htm:

- DIPAN 00 / 10 = true center; bits 0–3 select 3 dB attenuation steps
  on the channel chosen by bit 4 (0: left attenuated / pan right;
  1: right attenuated / pan left).
- DIPAN 0F mutes LEFT, 1F mutes RIGHT.
- DISDL 0 disables direct output; levels 1..7 select -36, -30, -24,
  -18, -12, -6 and 0 dB attenuation respectively.

We added an owner-ROM-only SCSP register probe to
`tests/m68k/test_l3_real_rom_selections.c`. From original 68000
execution at the first note on tick 0, all eight voices had **DISDL=3
(-24 dB)**, six had **DIPAN=00** (center), two had 13/14 (leftward).
At tick 882, 11 of 13 active voices were centered; at tick 4410,
13 of 17 were centered. These directly explain the false pan/volume
in the first preview.

`src/hardware/scsp.c` now scales each dry slot by DISDL *before*
mixing and uses a deterministic Q15 approximation to the DIPAN 3 dB
steps. ROM-free mixer tests cover center, left/right and mute. No
DSP/EFSDL/wet rendering is claimed.

The full original-ROM `--akira-sequenced-wav-dir` experiment was
rerun from scratch after the patch. It still executed all **8820**
experimental L2 ticks and drained **456** original 68000 commands;
the exact model-only sequence contained **441,000 stereo frames**
and had a peak absolute PCM of **8,344** instead of 32,768.

| Measurement | Previous model | Corrected direct mixer |
| --- | ---: | ---: |
| Clipped left samples | 31,548 | **0** |
| Clipped right samples | 22 | **0** |
| Right channel zero frames | 82.65% | **5.96%** |
| Left/right RMS | 15,476 / 2,961 | **1,142 / 1,122** |

The corrected technical WAV is normalized by +10.77 dB **only for
an optional owner-local listening copy**, with common L/R gain and
no new sample clipping; that normalization is NOT part of the
SCSP device model or the source ROM. Private WAV, ROM, and sample
data remain outside the repository.

This change fixes two *verified* mathematical mixer errors but does
NOT imply arcade-perfect sound: DSP/wet path, authentic hardware
envelopes, sample interpolation, and IRQ cadence remain under study.

## Not yet the full track

The WAV is **an initial eight-voice note-state preview**, not the
complete musical score. The first 16 experimental L2 ticks produced
no further queued notes after the initial 32 commands; this does not
mean the ROM lacks the melody: musical stream data include interval
and count bytes and the driver must advance its event scheduling.
Original timer cadence, voice release and SCSP/DSP analog fidelity
are **not** established by this static-state render.

The next recoverable steps are (1) map music-record header `0x10`
and stream activation on the native-C A0 path, using the original
68000 at `0x1e98` as oracle; (2) decode the stream countdown
transitions in L2 beyond tick 16; (3) interleave original voice
maintenance / SCSP render with the appropriate IRQ cadence.
Do not present a replay of the same frozen eight voices as
the full *Ride the Tiger* track.
