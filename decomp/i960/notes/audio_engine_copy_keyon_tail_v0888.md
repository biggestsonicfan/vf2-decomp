# v0888 — VF2 original 68000 `0xD72..0xD90` stream pointer + SCSP KEYON tail

Date: 2026-10-19. Continues v0887's **exact 48-instruction
`0xCB2..0xD72` sample-to-sound-RAM MOVEM page copy**.

## Original ROM measurement, no invented instructions

The owner's original VF2.1 `epr-17574.30` ROM, reconstructed with
`LOAD16_WORD_SWAP`, has the following **30 bytes** at
`0xD72..0xD90` (exclusive):

```
3829 0000      MOVE.W (A1),D4
2788 4002      MOVE.L A0,2(A3,D4.W)
278A 4006      MOVE.L A2,6(A3,D4.W)
1A35 4000      MOVE.B (A5,D4.W),D5
0005 0018      ORI.B #$18,D5
1B85 4000      MOVE.B D5,(A5,D4.W)
17BC 0080 4000 MOVE.B #$80,(A3,D4.W)
```

The next opcode is `4E75` (RTS) at `0xD90`. **The checkpoint is
before RTS**, after exactly seven new instructions.

With original interpreter inputs `A1=0x2800` (voice), `A3=0x2000`
(stream), `A5=0x100000` (SCSP), and low-word channel index from
`(A1)`, the tail stores final A0/A2 at stream+2/+6, ORs SCSP
channel's first byte with `0x18`, then sets stream+0 to `0x80`.
The `0x18` store invokes the existing SCSP model's KEYON semantics,
not a simulated hardware DAC or RTA audio oracle.

The measured v0887 representative original `0xCB2` source
addresses were `0xE631B2` (AD23) and `0xF04A72` (AD24);
after the first 512-byte page, source pointers become `0xE633B2`
and `0xF04C72`. Destination A2 becomes `0x10200`.

## Recovered native C, explicit and bounded

`vf2_sound_board_finalize_engine_copy_d72()` is the separate
recovery of these seven tail instructions:

- Accepts only a valid voice index and aligned preceding page
  completion pointers (must address mapped sample and sound RAM).
- Reads the voice's big-endian slot offset; accepts only aligned,
  modeled SCSP slots. Refuses invalid channels without partial writes.
- Files both end pointers in the stream record, read/OR/writes the
  original `0x18` bits through SCSP model, and sets stream flag
  `0x80`.
- Leaves the existing dispatcher `0xCB2` refusal intact. No
  unobserved post-RTS body, gameplay audio event or time-accurate
  hardware behavior is inferred.

`tests/m68k/test_engine_copy_page.c` now validates seven additional
original 68000 instructions, including full post-copy RAM, SCSP
state, stream pointers and a refusal-safety ROM-free test. That brings
the local **copy+tail window to 55 original instructions** per case
(`48+7`), 512 bytes uploaded, at a stop PC `0xD90`.

The tests additionally verify all 16 original MOVEM opcode triplets
and the seven-instruction tail's **30 source ROM bytes**.

## Validation and packaging

```sh
# ROM-free: standard CI (no proprietary assets)
ctest --test-dir build -R vf2_m68k_engine_copy_page_rom_free --output-on-failure

# Owner-local ROM:
build/vf2_m68k_engine_copy_page_tests /path/to/vf2/roms

# With -DVF2_ROM_DIR set:
ctest --test-dir build -R vf2_m68k_engine_copy_page_differential --output-on-failure
```

The CI workflow previously published four sound-differential binaries
and inadvertently omitted `vf2_m68k_engine_copy_page_tests`.
This omission was repaired in `.github/workflows/ci.yml`, so the
next GCC GitHub Actions `vf2-m68k-sound-differential-linux` artifact
is expected to include the new executable for a real owner-ROM
differential.

**Status**: raw opcode bytes and the original source-address context
verified against the user's local ROM. The recovered C and differential
test are committed; the current post-tail binary **has not yet been
executed** on the owner's ROM. Do not call 55/55 differential passing
until that executable is actually tested. The ROM, snapshots, sound
buffers and WAVs are never committed.
