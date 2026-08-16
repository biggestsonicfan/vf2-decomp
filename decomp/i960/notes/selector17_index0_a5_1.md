# Selector 17 bit-7 index 0: second diagnostic visit

After the all-good ROM/RAM diagnostic completes, the next frame reaches the
same selector-17 bit-7 index-0 dispatch with `0x005000a4 = 0x80`, but
`0x005000a5` has advanced from zero to one.

At `0x00059154`, clearing bit 7 selects table entry zero at `0x0005fea8`.
The entry points to `0x00059164`, which uses `0x005000a5` as a secondary
selector. With `a5 = 1`, the table at `0x00059178` selects `0x00059358`.
That routine evaluates `0x00500704 & 0x04000104`; the observed value is zero,
so it returns immediately without rerunning the ROM/RAM tests.

ROM measurement from the `0x0000a6c0` frame-dispatch boundary to
`0x0000a010` is 36 instructions, 2 procedure calls, and 3 procedure returns.
The path does not change gameplay or tile output. The only memory normalization
observed at this boundary is the three-byte spill state at `0x005ff600..602`,
which becomes `00 00 56`.

The recovered implementation keeps the same strict selector/index/input guards
as the first visit and accepts only the measured `a5 = 1` branch. Other
secondary selector values remain unsupported until measured against the ROM.
