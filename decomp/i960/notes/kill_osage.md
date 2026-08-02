# `fa_kill_osage` recovery

`fa_kill_osage` enters at `0x000657dc` and returns after 36 interpreted
instructions. It calls helper `0x00065838` once for each `fa_osage` runtime
record.

The complete semantic C recovery:

1. reads the two osage runtime-record pointers from `0x00500868` and
   `0x0050086c`;
2. selects their processing order from bit 0 of `0x00500020`;
3. derives elapsed age from timer 3 using reload `0x000fffff`, startup bias 18,
   and divisor 25;
4. checks continuation `0x0006428c`, active flags and accumulated age at record
   offset `0x128`;
5. sets or clears kill flag bit 3;
6. increments global counter `0x00500164` when a record crosses threshold
   `0x00004268`.

The original and recovered task produce identical modeled memory at the end of
the first real scheduler dispatch. In that captured state both records are
examined and neither crosses the kill threshold.
