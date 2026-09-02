# v0222 high-29 low-cube for 0x8140

Extends high-26 family with high-29.

## Measurement
* `0x20008140|low` 8 masks `36/36 exact` with `-5/-3` and `hybrid_set_stale_low`
* Same as v0221

## Recovery
One predicate `(combined & ~0x16)==0x20008140`

55/55 CTest pass, +8 masks total 168.
