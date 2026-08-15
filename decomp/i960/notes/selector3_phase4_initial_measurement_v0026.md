# Selector-3 phase-4 measurement (v0.0.26)

This note records the controlled ROM corridors used to recover selector-3 phase 4 (`0x0000b3f8`).

The initial non-terminal route from framed `0x0000a6c0` executes **468,818 instructions / 20 calls / 21 returns**. Its major call boundaries are `0x24314` (225,337 instructions including its nested calls), `0x23ee8` (229,642), `0x23f30` (967), the large `0x8ef0` clear (12,147), `0x54820` plus two `0x549e8` actor records (166 / 3 / 3), optional `0x8f1c` (376), the second small `0x8ef0` (16), `0x548cc` plus four `0x54c14` timeline entries (98 / 5 / 5), and the `0x1344` wrapper exit (9).

The recovered source models the mode initialization performed by `0x24314`, the observed actor initialization/record path through `0x54820`/`0x549e8`, and the first timeline update through `0x548cc`. Synthetic unit-test ROMs that do not provide the real mode-init tables retain the older reduced fixture path via an explicit table preflight rather than fabricated table data.

Differential inspection recovered two details hidden by the older generic display helper. The phase-4 `0x8f1c` call targets `0x010016da`, not the helper's default `0x01004000`, and is followed by a 2x1 `0x8ef0` fill at `0x01000ef4`. The real `0x8f1c` prologue also leaves saved `g9=0x010016da` in the nested stack spill slot; the recovered path reproduces that architecturally visible spill. The non-terminal path is full CPU+memory snapshot MATCH.

## Terminal route

A controlled ROM vector with the phase-4 timer source set to one reaches the terminal branch after the same initial work. It executes **468,870 instructions / 21 calls / 22 returns**, exactly **52 instructions and one call/return** more than the non-terminal route.

The terminal tail clears `0x0050009c`, updates both fighter flag words by clearing bits 23 and 22 and setting bit 26, enqueues sound event `0x00bd1a60` through the already recovered `0x43888` queue semantics, clears bit 16 of `0x00508000`, and advances phase 5 -> 6. The phase-3 sound helper was generalized to take the event word so both phase 3 (`0x00ad1001`) and phase 4 (`0x00bd1a60`) use the same recovered queue implementation.

Relevant implementation commits are `9bc983919933cf9c6f8fb881f7a6feb8b7a15beb` (initial phase-4 recovery), `3b2a63defdb39f6e2eccb4177389cfd035638fdb` (display targets), `d2a38241010d57dd6ce67b43bd320f1f6ae45e0a` (proven `0x8f1c` spill), and `1e5d5b07b4985552650c8f9fbe628873d72aefaa` (terminal route). Both phase-4 outputs are validated against controlled ROM snapshots using the normal CI-built analysis binary.
