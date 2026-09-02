# v0223 high-30/31 low-cube for 0x8140

Extends high-26/29 family with high-30/31.

## Measurement
* `0x40008140|low` and `0x80008140|low` each 8 masks `36/36 exact` with `-5/-3`

## Recovery
Two predicates `(combined & ~0x16)==0x40008140` and `==0x80008140`

55/55 CTest pass, +16 masks total 184.
