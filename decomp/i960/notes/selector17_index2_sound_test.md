# Selector 17 bit-7 index 2: sound test

A controlled phase-17 transition from normal phase index 1 uses the measured
`0x00500704 = 0x00001004` forward+reset combination. The frame-final path
advances phase 1 to phase 2 and immediately re-arms bit 7, producing
`0x005000a4 = 0x82`, `0x005000a5 = 0`, and `0x005000a6 = 0xff`.

The bit-7 table entry at `0x0005feb8` points to `0x00059800`. The observed
first visit is the SOUND TEST screen. With the normal input state
(`0x00500700 = 0x0ff7f700`, navigation/released flags zero, previous input
matching, selector mask `0x00020000`), it renders `No.  0   Advertise`, followed
by the instructions to select with the player-1 lever, play with PUNCH, and
exit with TEST.

Reference execution from `0x0000a6c0` to `0x0000a010` consumes 1,844 i960
instructions, 12 procedure calls and 13 procedure returns. Outside the SOUND
TEST tile writes, the only writable-memory changes are the observed stack spill
normalization at `0x005ff600..0x005ff602` (`00 00 56`).

The first recovered case is intentionally exact to this measured entry state.
Lever selection, PUNCH playback and TEST teardown are tracked as separate
input-dependent branches so that unsupported sound-board behavior is not
silently generalized from the idle screen.
