# v0889 — Alternate 68000 512-byte upload / SCSP KEYON at 0xDF8..0xED6

Date: 2026-10-19. This is a **second isolated sound-driver
corridor**, not a live combat sound claim. Proprietary ROM bytes,
traces and WAVs remain owner-local.

## Measured instruction identity

Original VF2.1 sound CPU ROM `epr-17574.30`, reconstructed with
`LOAD16_WORD_SWAP`, contains:

| Entry | Copy-body range | Number of MOVEM triplets | Tail range | End | Stream flag |
| --- | --- | ---: | --- | --- | --- |
| Main copy branch | `0xCB2..0xD72` | 16 | `0xD72..0xD90` | RTS at `0xD90` | `0x80` |
| Alternate copy branch | `0xDF8..0xEB8` | 16 | `0xEB8..0xED6` | RTS at `0xED6` | `0x81` |

Each copy body contains exactly 16 consecutive original
`MOVEM.L (A0)+,D0-D7 / MOVEM.L D0-D7,(A2) /
ADDA.W #0x20,A2` triplets: **48 instructions**, **512
bytes** copied from the mapped sample ROM to sound RAM. This includes
the now verified legal register-to-memory MOVEM opcode `48D2`.
Both seven-instruction tails store A0 and A2 to stream+2/+6,
read-OR-write SCSP slot first byte with `0x18` to KEYON, then
differ **only in their final flag write** (`0x80` vs `0x81`).
The ROM bytes for all triplets and each tail have been independently
checked locally.

## Recovered C and test contract

`vf2_sound_board_copy_engine_page_512` is intentionally shared
by the two identical 512-byte bodies. The original
`vf2_sound_board_finalize_engine_copy_d72` and new
`vf2_sound_board_finalize_engine_copy_eb8` call the same
bounded internal original-tail routine, differing only in the
observed stream status byte. Invalid/unmapped RAM/ROM inputs and
misaligned/nonexistent slots continue to fail closed.

The expanded `tests/m68k/test_engine_copy_page.c` asserts:
- original ROM opcode words of both **16x MOVEM** bodies;
- exact original tail bytes (`0x80` vs `0x81`);
- ROM-free correct final flags, pointer writes, SCSP KEYON and
  refusal with zero partial stream mutation;
- for **AD23** and **AD24** table-selected source addresses on
  each branch: exact original CPU step count **48 + 7 = 55**,
  A0/A2, D0..D7, full 512-KiB sound RAM and modeled SCSP state.

This expands the differential from two to **four** isolated original
ROM-backed cases. Do not equate 4 fixtures with four naturally
heard sounds.

## Reproduction (owner ROM required)

```sh
cmake -S . -B build -DVF2_BUILD_TESTS=ON
cmake --build build --parallel

# ROM-free regression:
ctest --test-dir build -R vf2_m68k_engine_copy_page_rom_free --output-on-failure

# Original ROM differential (owner directory contains all sound chips):
build/vf2_m68k_engine_copy_page_tests /path/to/vf2/roms
```

The Linux artifact for `vf2-m68k-sound-differential-linux` published
by the CI can also be run against the owner's local ROM files.
The independent post-commit ROM differential must actually pass
before it is considered validated.

**Remaining:** the original `0xD92` setup arm, continuous
dispatcher integration, channel release/voice reaper, SCSP DSP
runtime/fidelity, and a genuine frame-advancing combat event from i960
through UART, 68000 and PCM. The dispatcher intentionally continues to
refuse at unproven boundaries rather than admit synthetic game state.
