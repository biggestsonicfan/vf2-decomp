# `fa_game_info` `0x1645c`: positive state-8 bit-6 high triples/quads

## ROM-backed measurement

The user-supplied 36-chip Virtua Fighter 2 ROM set was used locally as the
sequential i960 oracle. `compare-boot` and `native-fifth-dispatch` both matched
before probing this family. No ROM data or derived snapshot is committed.

The ten high-bit triples and five high-bit quads over bits 21, 26, 29, 30 and
31 were forced through the recovered child with state byte 8, flag bits 8+6,
no low bits 1/2/4, and the standard three fighter-record distributions. Every
mask was measured for countdown 0/1, mode bit 6 clear/set and thresholds 0, 1
and 2: 36 fixtures per mask, 540 fixtures total.

After recovering the shared dispatcher accounting and stale-frame
postcondition, all 15 masks match 36/36 exactly, including snapshot,
architectural signature and instruction/call/return counters.

The shared pre-correction native excess is:

- fighter-0-only: +3 instructions, except countdown=0/mode6=1 => +8;
- fighter-1-only: +3 instructions, except countdown=0/mode6=1 => +4;
- bilateral: +6 instructions, except countdown=0/mode6=1 => +7.

The ROM also leaves the same historical frame values for the entire family:
`r3=0x41000000`, `r4=0x07800f0f`, `r7=0x41000000`, and `r15=1` whenever
fighter 1 participates.

## Admitted masks

Triples: `0x24200140`, `0x44200140`, `0x84200140`, `0x60200140`,
`0xa0200140`, `0xc0200140`, `0x64000140`, `0xa4000140`, `0xc4000140`,
`0xe0000140`.

Quads: `0x64200140`, `0xa4200140`, `0xc4200140`, `0xe0200140`,
`0xe4000140`.

Together with the previously measured singles, pairs and all-five-high mask,
this closes every non-empty high-bit subset for the positive state-8 bit-6 +
bit-8 / no-low-bits family. Low-bit mixing outside the separately proven
all-five-high family remains fail-closed.
