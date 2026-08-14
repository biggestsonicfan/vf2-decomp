# Executable display runtime initializer (v0.0.26)

`0x0002eab8..0x0002ec20` is recovered as an executable caller of `display_transform_defaults` at `0x00031004`. An isolated ROM differential with valid work-RAM objects at `0x00500814` and `0x0050084c` executes 84 total instructions, 73 exclusive to the parent and 11 in the child, with one nested call and two returns.

The parent initializes the global display tuning words at `0x0050a160..0x0050a168`, normalizes the object pointed to by `0x00500814` across offsets `0xdf` and `0x234..0x2cc`, clears flags at `(*(u32*)0x0050084c)+0x40`, and calls the child. The child writes target transform `(6.0f, 4.7f, 18.5f)` at `+0x54`, zeroes the secondary triple at `+0x60`, and clears `+0x70`.
