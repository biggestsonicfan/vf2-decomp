# Game-info bit31 state-independent condition (v0147)

ROM-backed controlled probes showed that the positive-threshold bit31 post-condition does not depend on the fighter `+0xa00` state byte when both `+0x1a4` state-flag words are zero.

The previous v0146 recovery admitted only state 0/0. The v0147 sweep covered 196 state pairs drawn from `0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 15, 31, 127, 255`; every pair reached `0x00010dcc` with exact native/reference equality after removing the state-byte predicate.

State-4 corridors that alter the task return boundary remain separate and are not claimed by this recovery.
