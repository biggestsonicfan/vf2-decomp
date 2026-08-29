# Warm selector scheduler stack recovery (v0143)

After TEST exit and warm initialization, the main loop reaches `0x0000a010` at local-frame depth 3 with selector 2. The cold scheduler semantics are reused under the three persistent warm frames.

Warm scheduler entry (`0xa010` to `fa_game_info`) is ROM-validated at 1,467 instructions, 32 calls, and 30 returns. Scheduler transitions admit the historical depth 1 or warm depth 4; `fa_game_info` to `fa_camera` is exact at 448 instructions, 9 calls, and 9 returns.

The task-name formatter now writes only the eight styled characters (16 bytes). The ROM leaves the following word untouched; the regression test preserves a sentinel there.

Scheduler finish stores its stale scheduler/text frames relative to the live depth, preserving the three outer warm frames. `fa_osage1` return to `0xa014` is exact at 281 instructions, 4 calls, and 5 returns.

The complete selector-2 frame from warm `0xa010` to the next `0xa010` executes 204 blocks, 1,276,892 instructions, 421 calls, and 421 returns. Snapshot matches the ROM byte-for-byte and selector advances naturally from 2 to 3.

The specialized hot initializer scheduler no longer overwrites `r0` with a cold-stack constant after the executor has already constructed the correct procedure frame. This makes the warm selector-3 scheduler prefix exact through `0xa014` while retaining the cold behavior.

No ROM bytes or snapshots are committed.
