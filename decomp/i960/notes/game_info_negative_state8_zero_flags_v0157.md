# v0157: recover bilateral negative state-8 zero-flag condition

This checkpoint removes the remaining condition-state mismatch for the
bilateral state-8, zero-state-flag `fa_game_info` path under a negative shared
fighter threshold.

The recovered body was already exact. ROM-backed differentials showed that
memory, registers, calls, returns, saved-frame state and instruction accounting
all matched at the scheduler-return boundary. Only the final i960 condition
state differed:

- countdown zero: ROM leaves EQUAL;
- countdown nonzero: ROM leaves LESS.

The native path had left GREATER in both cases. The fix now applies the
countdown-derived condition only to the measured bilateral state-8 zero-flag
negative-threshold domain; the existing nonnegative zero-flag rule is unchanged.

Validation covered three distinct negative threshold bit patterns
(`0xbf19999a`, `0x80000000`, `0xffffffff`) crossed with countdown 0/1, for 6/6
exact full snapshots at `0x00010dcc`. The zero-countdown cases execute 680 task
instructions and the nonzero-countdown cases execute 656, matching the ROM
reference exactly.

The complete non-ROM CTest suite remains covered and passing. No ROM bytes or
generated snapshots are committed.
