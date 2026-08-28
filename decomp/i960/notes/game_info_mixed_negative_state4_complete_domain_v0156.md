# v0156: complete mixed negative state-4 measured flag domain

This checkpoint completes native recovery of the measured mixed negative
state-4 flag domain over bits 6, 14, 15 and 16.

The seven newly admitted masks are:

- bit6 | bit14;
- bit6 | bit16;
- bit6 | bit15;
- bit6 | bit14 | bit16;
- bit6 | bit14 | bit15;
- bit6 | bit15 | bit16;
- bit6 | bit14 | bit15 | bit16.

Together with the isolated and compound families recovered in v0149-v0155,
all 16 masks in the measured 4-bit domain, including the zero mask, now have a
ROM-backed native mixed-state4 path under a negative shared threshold.

Two final-condition families emerged from the i960 reference:

1. Masks without bit15 leave EQUAL when countdown is zero and LESS when
   countdown is nonzero.
2. Masks containing bit15 leave LESS when countdown is nonzero or when the
   state-4 fighter itself carries any selected flags; with countdown zero and
   flags only on the non-state-4 fighter they leave EQUAL.

The existing bilateral state-4 child bodies already matched the mixed-pair
memory/register/accounting effects for these masks. No additional bit11 cleanup
was needed for compound bit16 masks; that correction remains restricted to the
isolated bit16 family measured in v0152.

ROM-backed differential validation covered 160 snapshots for each of the seven
new masks, for 1120/1120 exact snapshots. Coverage includes both state-4
orientations, three flag distributions for peer states 0..3, countdown 0/1,
mode-bit6 0/1, and cross-checks against peer states 5, 6, 7, 8, 9, 10, 15 and
255.

CPU state, memory, registers, calls/returns and recovered accounting match the
i960 reference at the `fa_game_info` scheduler-return boundary.

No ROM bytes or generated snapshots are committed.
