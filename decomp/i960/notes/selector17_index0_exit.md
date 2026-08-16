# Selector 17 bit-7 index 0: diagnostic exit

After the ROM/RAM diagnostic has completed once, the secondary selector at
`0x005000a5` is 1. The selector-17 path reaches `0x00059358`, which masks
`0x00500704` with `0x04000104`. The measured canonical TEST-button state uses
`0x00500704 = 0x00000004` and takes the branch to `0x0005f140`.

The exit path clears the 64x48 diagnostic tile plane, clears bit 7 from
`0x005000a4` (`0x80 -> 0x00`), restores the current phase marker, redraws the
12 standard phase text records plus the three extra records referenced through
`0x0005ff08`, `0x0005ff14`, and `0x0005ff18`, and returns to the normal
`0x0000a010` post-cluster boundary.

Reference measurement from `0x0000a6c0` to `0x0000a010`:

- 14,308 i960 instructions;
- 18 procedure calls;
- 19 procedure returns.

The observed poststate keeps `0x005000a5 = 1` and `0x005000a6 = 0xff`, while
`0x005000a4 = 0`. The stack spill bytes at `0x005ff600..0x005ff602` normalize
to `00 00 56`. The recovered implementation accepts only the measured
`0x00000004` navigation value for this exit; the other bits admitted by the
ROM mask remain unsupported pending explicit differential measurements.
