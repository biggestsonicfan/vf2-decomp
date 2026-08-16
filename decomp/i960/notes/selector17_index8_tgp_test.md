# Selector 17 / bit-7 index 8: TGP TEST

ROM slot `0x0005fee8` targets entry `0x0005ed0c`.  The handler uses four
`a5` states at `0x5ed30`, `0x5ee10`, `0x5ee44` and `0x5ef14`.

The recovered state machine is now complete for the measured TEST-mode
configuration.  State 0 draws IC.47/56/60/64 and PLEASE WAIT, builds the
32-entry one-hot table at buffer RAM `0x00918004..0x00918080`, stores the live
geometry pointer at `0x008000f0`, arms a 64-frame countdown and advances to
state 1.  State 1 decrements the countdown and advances to state 2 at 1.
State 2 reads video-control words `0x00980010` and `0x00980014`; for bits 0..3
of each word it renders GOOD when clear and BAD when set, then displays
PUSH TEST BUTTON TO EXIT and advances to state 3.  State 3 idles or exits via
mask `0x04000104`, restoring TEST MENU index 8 through the common teardown.

Reference measurements from the clean `0x0000a6c0 -> 0x0000a010` boundary:

- state 0 build: 663 instructions / 7 calls / 8 returns;
- state 1 countdown stay: 35 / 2 / 3;
- state 1 terminal 1 -> state 2: 37 / 2 / 3;
- state 2 TGP/video result render: 755 / 19 / 20;
- state 3 idle: 36 / 2 / 3;
- state 3 TEST exit: 14,308 / 18 / 19.

The observed video words in the captured runtime were both `0x33333333`, but
the native implementation follows the ROM bit test generically rather than
hard-coding that snapshot.
