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

Controlled lever measurements establish the selector boundary and two concrete
names. Clearing the phase-17 input bit 12 while retaining bit 13 selects UP from
index 0, wraps to index 296 and renders `No.296   Wo13`; this path consumes
1,482 instructions, 11 calls and 12 returns. Clearing bit 13 while retaining
bit 12 selects DOWN, advances to index 1 and renders `No.  1   stage clear`;
this path consumes 1,538 instructions, 11 calls and 12 returns. The selector is
stored at `[0x00500864] + 0x80`.

Two measured PUNCH encodings (`0x00500704 = 0x10` and `0x100`) both enqueue the
same index-0 sound command `0x00ad1001` and advance the sound queue cursor from
3 to 4. They consume 1,873 and 1,872 instructions respectively, each with 13
calls and 14 returns.

TEST (`0x00500704 = 0x4`) first enqueues five measured stop commands
`0x008e2950`, `0x008e2e7f`, `0x008d2250`, `0x00891e32`, and `0x00ae101f`,
advancing the queue cursor to 8. The helper then returns non-equal and
`0x00059804` branches to the shared teardown at `0x0005f140`. The full path to
`0x0000a010` consumes 14,450 instructions, 24 calls and 25 returns, clears bit
7 (`0x82 -> 0x02`), restores the phase menu and leaves the five stop commands in
the measured sound queue state.

Recovery remains deliberately finite-state: idle, the two single-direction
lever states, the two measured PUNCH bits, and TEST are accepted individually.
Unmeasured simultaneous control combinations remain explicit unsupported cases.
