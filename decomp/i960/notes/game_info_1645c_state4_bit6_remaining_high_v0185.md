# v0185: close state-4 bit6 compound isolated-high matrix

Entry `0x0001645c`, both fighter state bytes 4. This recovery closes the five remaining low families containing bit 6 combined with one isolated high bit 21, 26, 29, 30, or 31:

- `0x8040` (bit6+bit15): subtract 3 unilateral / 6 bilateral;
- `0xc040` (bit6+bit14+bit15): add 2 unilateral / 4 bilateral;
- `0x14040` (bit6+bit14+bit16): subtract 1 unilateral / 3 bilateral;
- `0x18040` (bit6+bit15+bit16): add 4/8, except high-29 add 3/6;
- `0x1c040` (bit6+bit14+bit15+bit16): add 2/4.

All families restore stale-frame `r3=0x41000000`, `r4=0x07800f0f`, `r7=0x41000000`, plus `r15=1` when fighter 1 participates. `0x14040` uses countdown-derived compare (`EQUAL` at zero, `LESS` otherwise); the other four families finish `LESS`.

`0x18040` additionally sets fighter `+0x1a4` bit 11 for participating fighters and, for high-29, writes `0x1e` to fighter `+0x6da`.

## Verification

Fresh VF2 V2.2 ROM-backed differential validation covered 25 masks x 3 distributions x 2 countdown values x 2 mode-bit-6 values x 4 thresholds (`-1`, `0`, `1`, `2`): **1200/1200 exact snapshots**. The strict warnings-as-errors build and local CTest suite pass **23/23**.

Together with v0177-v0184, this closes the tested 80-mask state-4 isolated-high extension matrix.
