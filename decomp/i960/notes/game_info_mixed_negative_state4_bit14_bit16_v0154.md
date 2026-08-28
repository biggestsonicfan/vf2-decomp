# v0154: mixed negative state-4 bit14|bit16 recovery

This checkpoint promotes the mixed negative-threshold state-4 corridor with
combined fighter state flags bit 14 and bit 16 into the recovered native C
path.

ROM-backed differential setup:

- exactly one fighter state byte at `+0xa00` is 4;
- shared fighter threshold is negative;
- `fighter + 0x1a4` combined flags are exactly bit14|bit16;
- the measured matrix distributions are retained;
- countdown values 0 and 1 are both covered.

The existing state-4 bit14|bit16 child was already structurally correct for
mixed pairs. The missing architectural rule was the final condition code:

- countdown == 0 leaves EQUAL;
- countdown != 0 leaves LESS.

The validation matrix reused the state/orientation/distribution coverage from
the preceding isolated-bit recoveries and produced 160/160 exact snapshots.
CPU state, memory, registers, calls/returns and recovered accounting match the
i960 reference at the `fa_game_info` task boundary.

No ROM bytes or generated snapshots are committed.
