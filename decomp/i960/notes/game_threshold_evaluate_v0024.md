# v0.0.24 game threshold evaluation

## Observed path

The complete function at `0x000028d4` is reached twice. It reads the current game-mode bytes, performs selector-zero and selector-one calls to the shared recovered color lookup, extracts the relevant color byte, calls the shared game-state classifier, and evaluates the observed below-nine quotient/offset threshold. Both visits return `g0 = 0` and `g1 = 0`.

## Composition

The wrapper contributes 38 instructions per visit and reuses the existing `game_color_lookup` and `game_state_classify` implementations. Their nested instruction, call, return and stack-write accounting is folded into the wrapper report rather than duplicated. Two wrapper checkpoints replace four standalone color-lookup and two standalone classifier checkpoints.

## Validation

The unit test supplies synthetic lookup tables and runtime state, verifies both nested color calls, the classifier, stack writes, final globals, instruction count, five internal calls, six returns and the complete outer procedure return.

The exact VF2 2.1 ROM-backed `native-second-dispatch` executed 125 reference instructions for each visit and reached complete CPU and Model 2 memory `MATCH`.

Strict totals are now 1,270,074 recovered and 748 interpreted bridge instructions, 176 recovered blocks and memory checkpoints, and 288 / 302 recovered calls and returns.
