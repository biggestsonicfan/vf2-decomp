# v0881 — Export SCSP-model PCM from original 68000 voice key-on probe

Use the original-ROM 68000 stepper's sound-board state (including the
actual SCSP key-on) to produce **local, optionally saved stereo WAVs**
for the AD23 and AD24 representative packets.

The ROM-dependent binary `vf2_m68k_90_to_voice_tests` now supports:

    vf2_m68k_90_to_voice_tests /path/to/vf2 --wav-dir /path/to/output

The directory must already exist. In this opt-in mode, each command
renders 22,050 frames (0.5 s at SCSP 44,100 Hz) through
`vf2_scsp_render`, prints PCM peak / nonzero stereo frame count,
and saves signed 16-bit stereo RIFF/WAV with explicit little-endian
header and sample encoding. Default CTest and plain
`--probe-engine` behavior are unchanged. No proprietary ROM,
sample, or WAV is uploaded or committed to GitHub.

The filenames are `vf2_ad23_9b4278_scsp_model.wav` and
`vf2_ad24_9a017f_scsp_model.wav`. This represents audio generated
by the **project's software SCSP model**, with key-on authored
by executing the original VF2.1 68000 ROM in a controlled test.
It is *not* a captured analog arcade-board output, nor evidence
of a naturally triggered in-game hit.

CI checks only ROM-free compilation; use owner-local files and
the published Linux executable to confirm actual WAV output.
