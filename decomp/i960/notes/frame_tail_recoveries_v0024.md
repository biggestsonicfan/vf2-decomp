# v0.0.24 frame-tail recoveries

## Recovered paths

This batch recovers seven complete observed functions from the second-dispatch frame tail:

- `0x0006dcb8` executes the four-byte `0x55`/`0xaa` memory probe, restores every probed byte, and increments the mirrored 64-bit diagnostic counter.
- `0x000110f4` synchronizes the two input latch words and updates the observed runtime video flags.
- `0x000112f8` advances the frame counter and follows the observed non-boundary phase path.
- `0x00011c78` advances the packed phase nibble and mirrors it to both hardware and shadow storage.
- `0x00000530` verifies nine live bytes against the corresponding shadow bytes.
- `0x000110b0` loads the two active frame-buffer pointers and follows the observed clear-bit gate.
- `0x0000a6c0` mirrors the frame selector and executes the observed countdown callback at `0x0000a974`.

Unsupported branches remain rejected. The nested memory probe and countdown callback are accounted as procedure calls and returns without duplicating their semantics elsewhere.

## Validation

The public bridge tests cover the memory-probe counter mirrors, input flags, counter and phase updates, nine-byte shadow verification, frame-buffer pointer loads, callback table target, countdown write, architectural returns, and instruction/call/return accounting.

The exact VF2 2.1 ROM-backed `native-second-dispatch` reached complete CPU and Model 2 memory `MATCH`. The strict totals are now 1,270,352 recovered and 470 interpreted instructions, 195 recovered blocks and memory checkpoints, and 297 / 315 recovered calls and returns.
