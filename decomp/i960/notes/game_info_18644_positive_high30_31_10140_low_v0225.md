# v0225 high-30/31 low-cube for 0x10140

Extends high-26 family.

## Measurement
* `0x40010140|low` and `0x80010140|low` each 8 masks `36/36 exact` with `+8/+4` plus bit11
* `0x20010140|low` remains `0/36` with `+2` diff, stayed fail-closed

## Recovery
Two predicates `(combined & ~0x16)==0x40010140` and `==0x80010140`

55/55 CTest pass, +16 masks total 208.
