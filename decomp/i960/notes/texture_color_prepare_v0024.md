# v0.0.24 texture color preparation

The prefix at `0x0004cd18` mirrors the timer gate used by the word path, calls the shared recovered helper at `0x00000b6c`, verifies the zero child state and observed flag branch, loads the decoded texture dimensions and prepares the color-conversion loop at `0x0004cdb0`.

The wrapper contributes exactly 21 instructions per visit. Across four visits it recovers 84 previously interpreted instructions and four procedure calls. The nested timer helper remains shared and contributes its own recovered instructions and return to each report.

The unit test verifies timer writes, dimensions, `g10=2048`, register/comparison post-state and frame accounting. The exact VF2 2.1 ROM-backed `native-second-dispatch` reached full CPU and Model 2 memory `MATCH`.

Strict totals are now 1,269,998 recovered and 824 interpreted bridge instructions, 180 recovered blocks/checkpoints and 282 / 300 recovered calls/returns. All standalone timer-helper checkpoints have been absorbed by the two typed preparation wrappers.
