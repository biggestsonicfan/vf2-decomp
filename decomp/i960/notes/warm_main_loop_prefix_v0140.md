# Warm main-loop prefix recovery (v0140)

The warm post-boot corridor now continues from `0x00009a00` to the coprocessor initializer entry at `0x0000a178` using the ROM-observed initialization path rather than the previous synthetic jump to `0x00009fb0`.

ROM-backed validation of the supplied supported VF2 Version 2.1 image measures this boundary at exactly 1,676 instructions, two procedure calls, and one procedure return, entering `0x0000a178` at local-frame depth 4. The recovered snapshot matches the ROM reference byte for byte.

A major discrepancy was traced to the routine at `0x00019a7c`. The ROM sets `0x00548005` to 1 and initializes 480 service slots, each `0x40` bytes apart, by storing `0xffffffff` at slot offset `0x30`. The previous model wrote 3, iterated 510 slots, and stored zero. Correcting those semantics accounts exactly for the previously observed 1,921 Work RAM byte differences.

The recovery also reproduces the fixed warm-main globals, player/input defaults, render values, DIP/profile clamps, runtime flags, and caller-local values produced before the `0x0000a178` call. The former 700,000-instruction shortcut is removed from this measured path.

No ROM bytes or snapshots are committed.
