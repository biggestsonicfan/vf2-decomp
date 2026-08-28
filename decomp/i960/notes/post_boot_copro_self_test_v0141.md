# Post-boot coprocessor self-test recovery (v0141)

The warm initialization corridor now recovers the complete `0x0000a178` coprocessor/self-test block through its return at `0x00009f74`.

The implementation composes the existing recovered inline-text thunk at `0x00009444`, the two ROM-observed calls to helper `0x0000a3d4`, the two work buffers at `0x0050e000` and `0x0050e800`, their comparison, and the final diagnostic text sequence. The `0x0000a3d4` helper reproduces its command writes and reads through the Model 2A coprocessor port at `(g11)[g12]`, including the byte cursor at `0x005001e4` and buffer output at `0x0090e000`.

ROM-backed validation against the supplied supported VF2 Version 2.1 image measures `0x0000a178 -> 0x00009f74` at exactly 13,324 instructions, 10 procedure calls, and 11 procedure returns. The recovered snapshot matches the ROM reference byte for byte, including CPU/local-frame state, Work RAM, buffer RAM, tile RAM, and coprocessor-port backing.

The successful path remains fail-closed if the two generated result buffers differ or if the expected warm call depth / coprocessor port address is not present.

No ROM bytes or snapshots are committed.
