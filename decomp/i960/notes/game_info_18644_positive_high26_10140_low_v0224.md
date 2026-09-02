# v0224 high-26 low-cube for 0x10140

Extends `0x10140` low cube with high-26.

## Measurement
* `0x04010140|low` 8 masks `36/36 exact` with `+8/+4` and bit11 + stale

## Recovery
One predicate `(combined & ~0x16)==0x04010140`

55/55 CTest pass, +8 masks total 192.
