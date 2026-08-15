# Selector-3 phase-4 initial-path measurement (v0.0.26)

This note records the controlled ROM corridor used to recover the initial non-terminal selector-3 phase-4 path (`0x0000b3f8`).

From the framed `0x0000a6c0` entry, the measured route executes **468,818 instructions / 20 calls / 21 returns**. Its major call boundaries are:

- `0x24314`: 225,337 instructions including its call and nested returns;
- `0x23ee8`: 229,642;
- `0x23f30`: 967;
- `0x8ef0` display clear: 12,147;
- `0x54820` plus two `0x549e8` actor records: 166 / 3 calls / 3 returns;
- optional `0x8f1c`: 376;
- second small `0x8ef0`: 16;
- `0x548cc` plus four `0x54c14` timeline entries: 98 / 5 / 5;
- `0x1344` wrapper exit: 9.

The recovered source models the mode initialization performed by `0x24314`, the observed actor initialization/record path through `0x54820`/`0x549e8`, and the first timeline update through `0x548cc`. Synthetic unit-test ROMs that do not provide the real mode-init tables retain the older reduced fixture path via an explicit table preflight rather than fabricated table data.

Differential inspection also recovered two details that were hidden by the older generic display helper. The phase-4 `0x8f1c` call targets `0x010016da`, not the helper's default `0x01004000`, and it is followed by a 2x1 `0x8ef0` fill at `0x01000ef4`. The real `0x8f1c` prologue also leaves its saved `g9` value (`0x010016da`) in the nested stack spill slot; the recovered fast path reproduces that architecturally visible spill.

Relevant implementation commits are `9bc983919933cf9c6f8fb881f7a6feb8b7a15beb` (initial phase-4 recovery), `3b2a63defdb39f6e2eccb4177389cfd035638fdb` (display targets), and `d2a38241010d57dd6ce67b43bd320f1f6ae45e0a` (the proven `0x8f1c` spill). Full native-vs-ROM snapshot validation is performed on the CI-built analysis binary for this source state before this route is treated as closed.
