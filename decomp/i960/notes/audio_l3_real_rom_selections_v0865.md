# v0865 — Real-ROM selections into C-vs-68000 UART L3 differential

The user-owned VF2.1 main_data ROM at 0x0201e44c supplies
profile 1 / column 5 = 0x00AD2342 and
profile 9 / column 0 = 0x00AD2401. Neither observation
claims the game naturally issues these packets.

## New CTest

tests/m68k/test_l3_real_rom_selections.c reads **both** commands
from the original ROM pointer table (fail closed on mismatches)
and files each command byte-by-byte into two independently
initialized copies of the same initial sound-board state:

- recovered C: vf2_sound_board_file_uart_byte;
- reference: original 68000 code stepped by vf2_m68k_run
  from 0x52e to 0x5ea with d0 equal to the incoming UART byte.

After each of the six bytes, compare full sound RAM excluding
only the previously measured 68000 JSR return-address spill,
the modeled SCSP state, and exact timing of the completed ring
entry and 0x400001 system-port poke. After the third byte,
expect a four-byte entry AD 23 42 00 or AD 24 01 00,
write cursor 4, and packet count 1.

This adds coverage that the prior synthetic-chain test
(0x009a0001) did not: **real command-family input** to the
already recovered receiver. The test stops **before**
dispatch/voice-keying, because the 0xAD family takes the
A0 dispatcher whose live effects still require their own
independent evidence.

Build/reproduce with a supported local ROM directory:

    cmake -S . -B build -DVF2_ROM_DIR=/path/to/roms/vf2
    cmake --build build --config Debug --target vf2_m68k_l3_selection_tests
    ctest --test-dir build -C Debug -R vf2_m68k_l3_real_selections --output-on-failure

The new C test is registered only when VF2_ROM_DIR exists.
The shipped ROM is **never committed**. This turn verified both
profile/column commands using the supplied zip and executed
105/105 existing Python tests. A native C compile and the
new C-vs-68000 execution could **not** be run in this
isolated runtime (the entire repository build is not mounted).
Do not mark the test MATCH until that CTest passes.
