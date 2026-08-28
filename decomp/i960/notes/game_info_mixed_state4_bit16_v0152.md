# v0152 - Mixed negative state-4 bit-16 recovery

## Scope

This recovery promotes the measured mixed state-4, negative-threshold, isolated/bilateral bit-16 family from the interpreted fallback into the native `0x18644` C corridor.

The admitted family is intentionally narrow:

- exactly one fighter is in state 4;
- the shared fighter threshold is negative;
- the combined `fighter + 0x1a4` state flags are exactly bit 16;
- the flag may be on fighter 0, fighter 1, or both;
- countdown 0 and 1 are covered;
- mode bit 6 clear and set are covered.

## Recovered semantic detail

The generic native `0x18644` implementation was already architecturally correct except for the bit-16 state-word result. Its shared tail synthesized bit 11 in the recovered state word, producing `0x00010800` from an input `0x00010000` where the ROM preserves `0x00010000`.

For this measured mixed state-4 family, the native path now removes that spurious bit-11 result while preserving bit 16 and all other recovered state.

## ROM-backed validation

160 controlled differential cases match exactly at task return `0x00010dcc`.

The matrix covers:

- other-fighter states 0, 1, 2, 3 exhaustively across both state-4 orientations, all three bit-16 distributions, countdown 0/1 and mode-bit-6 clear/set;
- states 5, 6, 7, 8, 9, 10, 15 and 255 across both orientations with representative isolated/bilateral bit-16 distributions and countdown/mode extrema.

All compared snapshots match CPU state, local-frame state, mutable memory and execution accounting.

## Regression

The complete ROM-backed CTest suite remains green (52/52, with the long dispatch tail run in separate batches to stay within the interactive execution window).
