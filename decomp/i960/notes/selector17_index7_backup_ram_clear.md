# Selector 17 / bit-7 index 7: BACK UP RAM CLEAR

ROM entry `0x0005e848` is selected by phase index `0x87`.  Its `a5` jump table
contains five states at `0x5e870`, `0x5e924`, `0x5e948`, `0x5e990` and
`0x5ea80`.

This recovery cut covers the complete non-destructive selection UI.  State 0
draws `YES(CLEAR)` and `NO (CANCEL)`, selects NO and advances to state 1.  State
1 idles or accepts either `0x1000`/`0x2000` from helper `0x60b50`, advancing to
state 2.  State 2 swaps the cursor to YES and advances to state 3.  State 3
idles or accepts canonical `0x1000`, returning to state 0 so NO is rebuilt.

Native ROM measurements from the clean `0x0000a6c0` boundary are:

- state 0 build: 711 instructions, 7 calls, 8 returns;
- state 1 idle: 49 / 4 / 5;
- state 1 positive select: 51 / 4 / 5;
- state 1 negative select: 48 / 4 / 5;
- state 2 cursor swap: 43 / 2 / 3;
- state 3 idle: 41 / 2 / 3;
- state 3 positive return-to-NO: 38 / 2 / 3.

The actual destructive confirmation is deliberately not approximated.  At
state 3 the `0x04000104` path calls `0x6001c`, `0x5427c` and `0x5ff7c`, mutating
backup SRAM, its work-RAM mirror, randomized initialization records and CRC
state before displaying `COMPLETE` and entering state 4's countdown.  The state
1 cancellation path also enters the shared TEST MENU teardown.  Both remain
explicitly unsupported until their full persistent-memory/poststate effects are
recovered and strict-differentially validated.
