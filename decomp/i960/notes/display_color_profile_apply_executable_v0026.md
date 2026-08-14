# Executable display color profile apply (v0.0.26)

`0x0001fffc..0x00020050` is now recovered as an executable caller rather than folded into selector-3 synthetic poststate.

The procedure reads the ordinary profile byte at `0x00500064`, reads runtime flags at `0x00500068`, and forces profile `3` only when bit 21 is set. It then indexes `0x0006ee00 + profile * 0x100`, copies bytes `+0xb8..+0xba` to `0x005000e0..0x005000e2`, performs a real recovered call to `color_table_rebuild` at `0x00002c38`, verifies the architectural return to `0x00020050`, and finally returns to its own caller.

Exclusive instruction accounting is 12 instructions on the ordinary path and 13 instructions on the forced-profile path. The child color-table procedure contributes its independently validated `38,938 + sentinel_count` instructions. The parent contributes one nested call and, including the child and parent returns, two procedure returns.

This preserves actual i960 local-frame transitions through `vf2_i960_cpu_enter_procedure()` and `finish_recovered_procedure()` rather than merely adjusting counters.
