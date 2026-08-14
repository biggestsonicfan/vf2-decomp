# Executable display profile mode constants (v0.0.26)

`0x0001ff0c..0x0001fff8` is recovered as an executable caller of `display_profile_unit_fill` (`0x0001fee4`). The child uses the architectural i960 call frame and returns to `0x0001ff10` before mode dispatch.

Exact exclusive instruction counts from ROM disassembly are 5 instructions for the default branch, 8 for mode 10, and 27 for mode 6. These totals include the call instruction and parent `ret`, but exclude the child's independently recovered 108 instructions.

Mode 10 writes two tuning words. Mode 6 writes eleven tuning words. Other modes return immediately after the unit fill. The recovery also preserves the final `r15` value and comparison condition produced by the executed `cmpobe` path.
