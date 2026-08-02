# v0.0.24 frame and interrupt support batch

This batch recovers nine deterministic frame/interrupt support procedures plus the composite game-meter update at `0x000020f0`.

The support procedures cover geometry and player gates, packed video-register composition, input-latch and ring handling, layer commit, two sequence gates, and the tile runtime gate. The composite meter path reuses the recovered threshold and color helpers, preserves nested i960 frame behavior and temporary stack writes, and avoids duplicating lookup/classification semantics.

Exact VF2 2.1 ROM-backed `native-second-dispatch` validation reached complete CPU and Model 2 memory `MATCH`. The strict totals are 1,270,617 recovered and 205 interpreted instructions, 200 recovered blocks and memory checkpoints, and 309 / 332 recovered procedure calls and returns.
