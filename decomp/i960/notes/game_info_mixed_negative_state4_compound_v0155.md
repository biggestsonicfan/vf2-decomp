# v0155: mixed negative state-4 compound flag recovery

This checkpoint promotes three compound mixed state-4 negative-threshold
families into the recovered native `fa_game_info` path:

- bit15 | bit16;
- bit14 | bit15;
- bit14 | bit15 | bit16.

The existing bilateral state-4 child bodies were already structurally correct
for these masks. Mixed-pair ROM-backed probes showed that the missing rule is
the final i960 condition code and that it depends on both countdown and which
fighter carries the compound flags.

For each admitted family:

- countdown != 0 leaves LESS;
- countdown == 0 and the state-4 fighter carries any of the compound flags
  leaves LESS;
- countdown == 0 with the compound flags only on the non-state-4 fighter
  leaves EQUAL.

Exactly one fighter state byte at `+0xa00` is 4, the shared threshold is
negative, and the measured matrix distribution guard remains required.
Unmeasured masks continue to fail closed through the conservative fallback.

ROM-backed differential validation covered 160 snapshots per family, for a
total of 480/480 exact snapshots. Coverage includes both state-4 orientations,
three flag distributions for peer states 0..3, countdown 0/1, mode-bit6 0/1,
and cross-checks against peer states 5, 6, 7, 8, 9, 10, 15 and 255.

CPU state, memory, registers, calls/returns and recovered accounting match the
i960 reference at the `fa_game_info` scheduler-return boundary.

No ROM bytes or generated snapshots are committed.
