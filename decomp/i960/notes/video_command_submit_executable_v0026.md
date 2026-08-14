# Executable video command submit (v0.0.26)

`0x0004b410..0x0004b448` is now recovered as a standalone executable leaf. It writes the command-ready word at `0x00550000`, emits selector `3` at `0x005502e0`, and copies the caller's `g0/g1/g2` into the three packet arguments at `+4/+8/+12`.

The leaf executes exactly 9 instructions including its final `ret`, has no nested calls, writes 20 bytes, leaves `r3 = 0x005502e0` and `r15 = 3`, and otherwise preserves the caller's global argument registers.
