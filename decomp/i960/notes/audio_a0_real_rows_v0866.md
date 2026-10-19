# v0866 — Real VF2.1 AD23/AD24 A0 records and ROM-backed dispatch frontier

The previous v0865 test proved L3 receiver equality for original-ROM
sound selections; the 68000 A0 processing after reception remained open.

## Complete static ROM data-chain census

`tools/python/audio_a0_rows.py` reconstructs only the user-owned first
4 MiB main_data pair and 512 KiB sound CPU ROM, in the manifest's
LOAD32_WORD/LOAD16_WORD_SWAP order. It validates the 18×11 choice
matrix and reads A0 index (second command byte) from the sound ROM
BE32 pointer directory at 0x608e12, then the BE16 max, then the BE32
selected record pointer. It refuses invalid pointers / odd records.

Across the user's original VF2.1 ROM:

- 107 occupied selections and 98 distinct original commands.
- All 107 have a valid sound-ROM record; 98 distinct record addresses.
- 64 selections are 0x00AD23xx and 43 are 0x00AD24xx.
- The max selector is 0x7f for both index rows; 0 out-of-max cases.
- Command 0x00AD2342: row 0x60959c, record 0x60b4fe.
- Command 0x00AD2401: row 0x60979e, record 0x60b7f2.

**A pointer resolving is not a voice played.** This is the next
static record-lookup link, not gameplay reachability.

## Runtime comparison entry point

The existing v0865 C-vs-68000 test now continues **after** the
three UART bytes arrive. For each authentic command, a CONTROLLED
matching sound descriptor for channel 0xd is placed into identical
C/stepper sound-board states (never an invented live i960 producer).

- C `vf2_sound_board_dispatch_next` must explicitly refuse
  `VF2_ERROR_UNSUPPORTED` with `handler_address=0x1e98`
  and packet count held at 1.
- The original Motorola 68000 executor starts at 0x7a6,
  executes the switch and A0 max gate, and must arrive at
  **0x1e98 before executing the still-unrecovered body**.
- Selected sound-ROM record addresses and the max value are
  read and checked against ROM, as are stage/current command,
  count and SCSP equality at this bounded frontier.

This is *frontier pinning*, not C-vs-68000 equality through the
0x1e98 body. It deliberately does not assert audible audio.

Reproduce:

    VF2_ROM_ZIP=/path/to/vf2.zip python -m unittest discover \
      -s tools/python -p 'test_audio_*.py'
    python tools/python/audio_a0_rows.py /path/to/vf2.zip \
      --profile 1 --column 5 --json
    cmake -S . -B build -DVF2_ROM_DIR=/path/to/roms/vf2
    cmake --build build --target vf2_m68k_l3_selection_tests
    ctest --test-dir build -R vf2_m68k_l3_real_selections --output-on-failure

Validation in this turn: eight new Python tests passed including
real-ROM 107/98/98 assertions. The CTest was added but not executed
against the complete C project locally. Build/CI compilation success
does not prove the C-vs-68000 runtime gate; require CTest on user's
ROM set before promoting it.
