# v0887 — Recover MC68000 0xCB2..0xD72 first MOVEM upload page

Date: 2026-10-19. Scope: **512-byte isolated sample ROM -> sound RAM
copy**, not a full recovered sound engine or natural gameplay.

## Authoritative original ROM evidence

The user's original `epr-17574.30` reconstructed with the manifest's
`LOAD16_WORD_SWAP` contains **exactly 16 repetitions** of these 12
bytes over addresses `0xCB2..0xD72` (end exclusive):

```text
4C D8 00 FF    MOVEM.L (A0)+,D0-D7    ; read 8 x 32-bit values
48 D2 00 FF    MOVEM.L D0-D7,(A2)    ; write 8 x 32-bit values
D4 FC 00 20    ADDA.W #$0020,A2     ; advance destination +32
```

The earlier hypothesis that the second MOVEM traps was **incorrect**.
Motorola's M68000 PRM explicitly allows `MOVEM.L D0-D7,(An)`;
see `audio_movem_legal_mode_v0880.md`. Each group executes three
instructions, copies exactly 32 bytes, preserves D0-D7's last loaded
big-endian longwords, and advances A0 and A2 by 32. Sixteen groups
thus execute **48 instructions** and advance each pointer by
`0x200`. This is proven from original ROM opcodes, independent of
whether game events naturally activate the path.

In existing original-68000 driver probes on the user's ROM, the
two AD-family selected sample addresses at the `0xCB2` boundary
were `0x00E631B2` (AD23) and `0x00F04A72` (AD24);
both are in the attached `0x800000..0xFFFFFF` sample ROM window.
For destination A2=`0x10000`, the exact end addresses are:

| Representative | A0 before | A0 after | A2 after |
|---|---|---|---|
| AD23 / selector `0x0242` | `0xE631B2` | `0xE633B2` | `0x10200` |
| AD24 / selector `0x0261` | `0xF04A72` | `0xF04C72` | `0x10200` |

The sample ROM bytes were independently checked locally; neither
ROM input nor sample-derived output is included in this document.

## Implementation in recovered C

`vf2_sound_board_copy_engine_page_512()` is a **bounded,
explicitly invoked** recovered routine:

- validates that A0/A2 are aligned, A0 points inside the attached
  sample ROM, A2 points to sound RAM, and **all 512 bytes** are
  mapped before mutating anything;
- copies 16 32-byte pages between disjoint sample ROM and sound RAM
  (equivalent to MOVEM register stages for this bus map);
- advances caller-owned A0/A2 by 512 only on success;
- refuses unsupported addresses without modifying memory or pointers.

It is **not** hooked into `dispatch_next` yet, because the
post-`0xD72` body and continuous gameplay state have not been
proven and the existing `0xCB2` fail-closed boundary is load-bearing.

## Differential acceptance tests

`tests/m68k/test_engine_copy_page.c` adds both a ROM-free unit
and two original-ROM differentials:

```sh
cmake -S . -B build -DVF2_BUILD_TESTS=ON -DVF2_WARNINGS_AS_ERRORS=ON
cmake --build build --parallel
ctest --test-dir build -R vf2_m68k_engine_copy_page_rom_free --output-on-failure

# With original VF2.1 ROMs in directory form:
build/vf2_m68k_engine_copy_page_tests /path/to/vf2/roms
# Or configure -DVF2_ROM_DIR=/path/to/vf2/roms and:
ctest --test-dir build -R vf2_m68k_engine_copy_page_differential --output-on-failure
```

The ROM-backed test independently steps the **original Motorola 68000**
from `0xCB2` until **before `0xD72`**, asserting 48 executed
instructions and comparing **all 512 KiB of RAM**, original CPU
A0/A2, final D0-D7, and preserved SCSP state against recovered C.
It checks all 16 original instruction groups against the supplied
ROM bytes before running. The ROM-free tests pin atomic refusal of
bad alignment, absent ROM and out-of-range addresses.

**Status at commit:** original ROM triplet sequence and both sample
source windows have been measured locally; the new compiled C/68000
test is still pending an independent build-and-run as GitHub Actions
were queued. Never call the differential green before it actually
passes.

**Next frontier**: decode and measure `0xD72+` and the alternate
`0xDF8` engine leg; continue only at an original-ROM differential
boundary, then establish a true frame-progressing combat source.
