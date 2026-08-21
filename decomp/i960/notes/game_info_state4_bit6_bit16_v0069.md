# fa_game_info state4 + bit6 + bit16 — v0069

Recovered the combined `state4 + bit6 + bit16` corridor in `fa_game_info`.

Validation used the real `0x1645c` entry snapshot and three controlled fixtures: fighter 0 only, fighter 1 only, and both fighters. Each fixture preserved the real runtime state while setting `fighter+0xa00 = 4` and `fighter+0x1a4 = 0x00010240` for the selected fighter(s).

The existing native `0x18144` bodies already compose semantically for bit6 + bit16, including the state-4/bit16 state-bit-11 update. No new semantic behavior was required beyond dispatcher admission.

Measured end-to-end instruction counts before accounting were one instruction short in every orientation. The exact correction is therefore one fixed instruction when the combined `state4 + bit6 + bit16` corridor is admitted.

Final differential results to scheduler return `0x10dcc`:

- fighter 0: 903 ROM / 903 native
- fighter 1: 903 ROM / 903 native
- both fighters: 1073 ROM / 1073 native
- snapshots: exact in all three cases
- calls, returns, interrupt entries, and interrupt returns: zero delta

The temporary probe PR was not merged; the validated clean source candidate was fast-forwarded directly onto `master`.
