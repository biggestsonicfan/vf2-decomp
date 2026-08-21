# fa_game_info state4 + bit6 + bit14 recovery (v0068)

The combined state-4 + bit-6 + bit-14 corridor is now recovered natively in `hybrid_execute_game_info_bit31_native`.

## Observed corridor

Real same-chain snapshots were derived from the validated `0x1645c` entry state. The selected fighter(s) used:

- state byte `fighter + 0xa00 = 4`;
- state flags `fighter + 0x1a4 = 0x00004240` (`0x200 | bit6 | bit14`);
- bit15 and bit16 clear;
- the existing non-negative shared threshold guard.

The existing recovered `0x18144` bodies already compose correctly for bit6 + bit14. No new memory semantics were required; only dispatcher admission and exact instruction accounting were missing.

## Differential result

Full ROM vs native execution from `0x1645c` through scheduler return `0x10dcc` was validated for all three orientations:

| case | ROM instructions | native instructions |
| --- | ---: | ---: |
| fighter0 | 904 | 904 |
| fighter1 | 904 | 904 |
| both | 1075 | 1075 |

For all three cases:

- snapshots match;
- procedure-call count matches;
- procedure-return count matches;
- interrupt-entry count matches;
- interrupt-return count matches.

The measured accounting correction is one additional instruction for the combined state4 + bit6 + bit14 corridor, independent of whether one or both fighters select it.
