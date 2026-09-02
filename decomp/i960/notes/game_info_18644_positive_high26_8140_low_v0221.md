# v0221 high-26 low-cube for 0x8140

Extends the positive state-8 `0x8140` low cube with high bit 26.

## Measurement
* Base `0x04008140` (bits 6+14+15+26) with low cube over bits 1,2,4 => 8 masks `0x04008140|low`
* `validate_game_info_full_dispatch.py --mask --state 8` for all 8 masks `36/36 exact` with `-5 bilateral / -3 unilateral` and same stale `r3=0x41000000` etc
* Before, high-26 alone was `0/36` fail-closed; low variants not admitted

## Recovery
`src/recovered/hybrid.c` added one predicate `(combined & ~0x16)==0x04008140` with identical accounting and `hybrid_set_stale_low`.

55/55 CTest pass, 8 new masks, total 160.
