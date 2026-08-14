# Executable display profile unit fill (v0.0.26)

`0x0001fee4..0x0001ff08` is now recovered as a standalone executable procedure.

The ROM performs a 26-entry counted fill rooted at `0x0050a0e0`, writing IEEE-754 `1.0f` (`0x3f800000`) to each consecutive 32-bit slot. The procedure has no nested calls and returns directly to its caller.

Exact instruction accounting is 108 instructions including the final `ret`: three setup instructions, 26 iterations of four instructions each, and one return instruction.
