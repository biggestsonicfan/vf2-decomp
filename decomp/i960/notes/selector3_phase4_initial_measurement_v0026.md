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

The recovered source now models the mode initialization performed by `0x24314`, the observed actor initialization/record path through `0x54820`/`0x549e8`, and the first timeline update through `0x548cc`. Synthetic unit-test ROMs that do not provide the real mode-init tables retain the older reduced fixture path via an explicit table preflight rather than fabricated table data.

Implementation commit: `9bc983919933cf9c6f8fb881f7a6feb8b7a15beb` (`Recover selector3 phase4 initial path`). Full native-vs-ROM snapshot validation is performed on the CI-built analysis binary for this source state before this route is treated as closed.
