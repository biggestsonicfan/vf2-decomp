# v0.0.24 remaining bridge batch

This batch recovers six deterministic observed blocks: the game event queue writer at `0x000438ec`, texture maintenance at `0x0004b8d8`, upload dispatch at `0x0004ba80`, the orchestrator entry gate at `0x0004bd00`, active-record status setup at `0x0004bd5c`, and the stream-header call at `0x0004be6c`.

Across the observed second dispatch they replace 93 interpreted instructions, add seven recovered procedure calls and four recovered returns, and introduce twelve complete CPU/memory differential checkpoints. Unsupported branches remain rejected rather than generalized without evidence.

The exact VF2 2.1 ROM-backed validator must reach full CPU and Model 2 memory `MATCH` before these totals are accepted: 1,270,167 recovered, 655 interpreted, 188 checkpoints, and 295 / 306 recovered calls and returns.
