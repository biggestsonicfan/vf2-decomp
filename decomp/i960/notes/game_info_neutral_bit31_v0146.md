# Neutral game-info bit-31 condition recovery (v0146)

A ROM-backed `compare-game-info` probe from the validated long-run checkpoint exposed a remaining architectural mismatch when both fighter root flags include bit 31 while both fighter state bytes are zero, both `+0x1a4` state-flag words are zero, and the shared threshold is nonnegative.

The native path already matched the reference task completely in memory, registers and accounting: `0x0001645c -> 0x00010dcc`, 678 instructions, 12 calls and 13 returns. The only mismatch was the final i960 condition state: the reference leaves `EQUAL`, while the recovered path left `GREATER`.

The recovery therefore records only that measured postcondition for the neutral state pair and deliberately does not broaden the existing state-4/state-8 families.

Controlled probes covering bit 31 alone and bit 31 combined with bits 1, 2, 4, 6, 8, 14, 15, 16, 21, 26, 29 and 30 all match the reference after the correction.
