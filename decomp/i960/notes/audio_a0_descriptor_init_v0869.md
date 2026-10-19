# v0869 — Recover A0 0x1f04 32-byte descriptor initialization in native C

This implements a new **functional** slice of the original VF2.1
Motorola 68000 A0 sound dispatcher, rather than just mapping
pointer tables. Source ROM opcode words 0x1eee through 0x1f14
(20 words) were read and verified against the supplied VF2 zip:

- 0x1eee: JSR PC-relative to 0x1f04 (descriptor clear).
- 0x1f04: MOVEQ #31,d7; MOVE.B #0,(a4)+; DBF loop 32
  writes; SUBA.L #0x20,a4; RTS.
- 0x1ef2: MOVE.L a2,4(a4) with a2=selected record+1.
- 0x1ef6: MOVE.W #1,2(a4).
- 0x1efc: BSET #7,0(a4); 0x1f02 RTS.

The six candidate descriptors start at absolute sound-RAM 0x3020,
stride 0x20, and a clear first-byte bit7 marks availability.
The new internal header sound_a0_descriptor.h implements only
the supported first-free path. All-six-occupied is deliberately
still refused without mutation: ROM DBF fall-through appears to
reach 0x30e0 and must be investigated separately, not assumed
safe. Non-AD23/AD24 A0 payloads keep their existing fail-closed
behavior (including old AE live queue tests).

sound_board_dispatch_a0 in src/hardware/sound_board.c now:
1. Uses original row + byte2 indexed BE32 record pointer, checks
   the record is readable and has bit7 set;
2. Scans six candidates and zeros only the first free 32-byte slot;
3. Writes BE32 postinc pointer, BE16 one, and status bit7;
4. Completes as C VF2_OK with report boundary 0x1f02,
   allowing the existing drain to consume the packet.

ROM-free native C executable tests/m68k/test_a0_descriptor_native.c
compiles without proprietary data, tests all six placements for
both original AD23/AD24 pointer values (12 cases), and confirms
no state change on all-busy, undersized and null buffer cases.
**Locally compiled and executed with gcc -std=c11 -Werror: PASS.**

The previous real-ROM C-vs-68000 test was upgraded to STOP at
68000 PC=0x1f02 before RTS and compare the initialized descriptor
32 bytes and SCSP state to recovered C. It stages only a matching
sound-side descriptor and a controlled vacancy; it does *not*
simulate a natural i960 gameplay trigger. Runtime integration
of this ROM-dependent differential remains unrun locally without
the full build checkout. Do not claim it passes until CTest runs
on the owner's ROM set.

Build:
  cmake -S . -B build -DVF2_ROM_DIR=/path/to/roms/vf2
  cmake --build build --target vf2_m68k_a0_descriptor_tests vf2_m68k_l3_selection_tests
  ctest --test-dir build -R 'vf2_m68k_(a0_descriptor_native|l3_real_selections)' --output-on-failure

This is not yet SCSP voice key-on or audible combat SFX.
