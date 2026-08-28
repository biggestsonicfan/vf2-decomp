# Warm post-boot waits and geometry table (v0138)

Fresh ROM-backed validation now follows the natural TEST MODE exit into the warm boot rather than the historical selector-17 diagnostic fixture. The warm post-boot call chain carries additional local frames, so the already recovered geometry-pattern and geometry-table helpers are admitted only at the measured warm depths in addition to their existing cold/test depths.

The post-boot `0x00000f7c` waits also expose a timing detail hidden by the earlier compact runtime: when the luma, pattern, final and geometry-table waits reach the poll with frame byte zero, the reference receives a pending VBlank before evaluating the loop condition. The runtime now injects that pending VBlank only for the measured caller return addresses and only at warm depths 4 or 5. The existing recovered vector-12 handler then supplies the actual CPU, stack, RAM and interrupt side effects.

The reference interrupt frame preserves arithmetic-control condition `GREATER` in its saved AC slot. That slot is derived from the PRCB interrupt-stack pointer rather than hard-coded. After the measured warm wait returns, the live condition state is `NONE`. Measured instruction accounting is four additional instructions for the initial texture wait and seven for each subsequent warm post-boot wait.

Using the supplied supported ROM, the following boundaries match exactly, including CPU state, local frames, instruction/call/return/interrupt counters and mutable memory: `warm-b0 -> 0x000098c0`, `0x000098c0 -> 0x000098c4`, `0x000098c4 -> 0x00011864`, and `0x00011864 -> 0x000098cc`. The geometry-table body itself matches `0x000098c4 -> 0x00002edc` in 281 instructions, and the table plus frame-commit/wait path matches `0x000098c4 -> 0x00011864` in 385 instructions.

No ROM data or snapshots are committed.
