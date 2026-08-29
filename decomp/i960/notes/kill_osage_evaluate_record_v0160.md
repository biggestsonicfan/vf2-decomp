# `fa_kill_osage` record evaluator (`0x00065838`)

The ROM block at `0x00065838..0x000658a0` is the shared record evaluator called twice by `fa_kill_osage` (`0x000657dc`). It is already represented semantically by `kill_osage_evaluate_record()` in `src/recovered/tasks.c`; this note closes the previously unattributed ROM range.

ROM behavior:

- starts with local accumulated age `r7 = 0`;
- accepts only continuation `0x0006428c`, flag bit 0 clear, and flag bit 2 set for the age/kill path;
- loads the record age from `record + 0x128`;
- compares `elapsed + age` against `0x00004268`;
- on threshold reach, sets record flag bit 3 and increments the global counter at `0x00500164`;
- otherwise clears record flag bit 3 and adds the loaded age to the running elapsed value;
- returns to either caller at `0x0006582c` or `0x00065834`.

This matches the existing clean-room helper exactly, including the threshold path not adding the record age to the running accumulator. No ROM data is committed.
