# v0.0.24 texture word preparation

The prefix at `0x0004cb64` derives a masked timer threshold, calls the existing recovered helper at `0x00000b6c`, verifies the zero child state and observed flag branch, then prepares the word decoder at `0x0004cc28`.

The wrapper contributes 22 instructions when the masked timer equals `0x000fffff` and 23 otherwise. Across the four observed visits it recovers 91 previously interpreted instructions and four procedure calls. The nested timer helper remains a single shared implementation; its dynamic 12-instruction path and return are included in each wrapper report.

The unit test validates the exact timer writes, register and comparison post-state, frame transitions, instruction accounting and decoder addresses. The VF2 2.1 ROM-backed `native-second-dispatch` reached full CPU and Model 2 memory `MATCH`.

Strict totals are now 1,269,914 recovered and 908 interpreted bridge instructions, 180 recovered blocks/checkpoints and 278 / 300 recovered calls/returns. Four standalone timer checkpoints remain for the second texture helper.
