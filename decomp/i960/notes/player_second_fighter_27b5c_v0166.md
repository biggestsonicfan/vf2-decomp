# v0166: recover second-fighter 0x27b5c record expander

This checkpoint removes the bounded i960 bridge that previously covered the
second fighter's `0x0001428c -> 0x000142c0` aggregate record-expansion path.

The divergence was isolated to `0x00027b5c`, selector `0x0283`. The recovered
second pass had simplified all status bytes to the same one-word copy path.
The ROM actually uses a compact status stream with distinct behaviors:

- status 0/1: copy three words and finish the row;
- status 2: finish the row without copying;
- status 3: write the observed zero scratch value;
- status 4: copy one word and advance the data cursor by one word;
- status 5: copy one word and advance by the pointer-stream value times 4;
- status >5: copy one word and advance by the pointer-stream value times 12.

The destination rewind at `0x27cb8` is a fixed 240 bytes, followed by 36
`cvtri/stis` conversions. The recovered helper now preserves the resulting
`g2`, `g3`, `g5`, and `g6` cursor post-state.

ROM probing also showed that the second-fighter aggregate corridor is 9,745
instructions with 6 calls and 6 returns, rather than the 9,726-instruction
first-fighter total.

After switching fighter 1 (`0x00512980`) from the direct-i960 corridor to the
recovered C expander, full task validation remains exact:

- entry: `0x00013f08`;
- exit: `0x00010dcc`;
- instructions: 16,275;
- calls: 53;
- returns: 54;
- final snapshot: exact match against the i960 reference.

The next remaining second-fighter bridge begins later at `0x000143e4` and is
still explicitly bounded. No ROM bytes or generated snapshots are committed.
