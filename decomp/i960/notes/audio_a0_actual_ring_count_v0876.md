# v0876 — Correct A0 68000 dynamic parity: both sides already consumed the ring

Running real-ROM tests from the published Linux CI artifacts
against owner-local vf2.zip revealed that both implementations
match at the 0x1f02 descriptor initialization boundary:

- all 32 bytes of the selected stream descriptor equal;
- SCSP modeled state is byte-for-byte identical;
- C reported a handled command;
- actual original Motorola 68000 command count is ZERO,
  not one: the original drain has already consumed it
  before reaching the A0 helper's RTS frontier.

The previous assertion demanding stepper count=1 was an
incorrect test contract, not a recovered C semantic mismatch.
The strict test now requires stepper count=0, and retains the
existing full descriptor, register-boundary, SCSP and
original-ROM validations. No sound-board runtime change.

The next differential boundary is the L2 B/90 two-packet
emission and original 68000 runner at 0x2198.
