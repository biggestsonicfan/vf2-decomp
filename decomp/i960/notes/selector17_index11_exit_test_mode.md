# selector17 bit-7 index11 — EXIT TEST MODE

## Dispatch

- selector17 bit 7 index: `11` (`a4 == 0x8b`)
- selector table slot: `0x0005ff00`
- indirect target: `0x0005ef60`
- wrapper chain used by the recovered frame path: `0x0000a6c0 -> 0x00010b5c -> 0x00058fe0 -> 0x0005ef60`

This index is not a normal return-to-menu item. It begins the test-mode shutdown sequence and eventually branches directly back to the boot entry.

## Observed corridor

The recovered bridge intentionally covers the selector17/index11 corridor measured from the TEST MENU. Other branches inside the shared `0x0005ef60` routine that are selected by unrelated game modes remain outside this recovery.

Entry invariants used by the bridge include:

- flagged phase index `0x8b`
- selector table target `0x0005ef60`
- valid model state (`mode != 25`, low two base flags clear)
- first visit: `a5 == 0` and test-mode system flag bit 0 set
- continuation: `a5 == 0xff`

## Per-frame common work

Every observed invocation first runs the same bookkeeping path:

1. reconstruct the real i960 call frames for `a6c0 -> 10b5c -> 58fe0`;
2. call the recovered meter/bookkeeping update path (`0x20f0`);
3. recompute the CRC over the 15-byte block at `base + 0x3320` through `0x5ff54 -> 0x9480`;
4. store that CRC at backup SRAM `0x01d03300`.

Keeping the real call frames matters because the routine uses stack/local-window values later in the exit path.

## First visit (`a5 == 0`)

The routine:

- clears the 64x48 diagnostic tile plane at `0x01000000`;
- renders the ROM text record referenced by `0x0005ff1c`;
- writes countdown `320` to `0x00500024`;
- changes `a5` to `0xff`.

Measured aggregate for the complete frame dispatch tick:

- instructions: `13286`
- procedure calls: `27`
- procedure returns: `28`

## Continuation (`a5 == 0xff`)

Each normal frame decrements `0x00500024` and returns through `0x5ef60`, `0x58fe0`, `0x10b5c`, and finally the frame-dispatch wrapper.

Measured aggregate for a non-terminal continuation tick:

- instructions: `626`
- procedure calls: `25`
- procedure returns: `26`

The countdown therefore spends 319 continuation frames after the initial arm at 320 before entering the terminal branch.

## Terminal reset

When the decremented countdown is signed `<= 0`, the ROM does not return through the normal selector wrappers. The recovered terminal path reproduces the observed shutdown effects:

- writes `0x8000` to `0x00500082`;
- clears bit 15 from both layer words at `0x0100a00c` / `0x0100a00e`;
- clears transient byte `0x0050009c`;
- clears the diagnostic tile plane;
- clears bits 0 and 1 from the layer-control word reached through `0x0050081c`;
- zeros input/released/previous navigation words at `0x00500700`, `0x00500704`, `0x00500708`, and `0x0050070c`;
- writes zero to `0x00e80004`;
- runs helper `0x0006116c` and writes the four-word reset sentinel at `0x0059cfe0`:
  - `0x52455320`
  - `0x4e4c2053`
  - `0x4e204544`
  - `0x20514555`
- branches directly to boot entry `0x000000b0`.

Measured aggregate for the terminal frame:

- instructions: `13194`
- procedure calls: `27`
- procedure returns: `25`

The lower return count is intentional: the terminal path abandons the selector/wrapper return chain and jumps to boot.

## Recovery boundary

For selector17 index11, the observed TEST MENU exit sequence is functionally recovered end-to-end: first visit, countdown continuation, and terminal reset. The shared routine at `0x0005ef60` has other mode-dependent semantics elsewhere in the game; those should be recovered as their own callers are measured rather than being inferred from this test-mode path.
