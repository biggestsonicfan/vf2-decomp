# v0878 — Original 68000 L2 AD23/AD24 termination leaves 0xFF in descriptor byte +1

First real-ROM differential of original 68000 worker versus
the recovered C L2 B/90 packet expansion reached **0x2198** for
both 0x00AD2342 and 0x00AD2401 (89 executed instructions).

Measured equal at that boundary:
- two emitted command packets in the ring;
- ring write cursor 0x000c and pending count 2;
- descriptor status byte0 == 0x00 and timer word == 0.

FIRST differing byte was stream descriptor +1:
- previous C: 0x9B / 0x9A (second command class);
- original 68000: 0xFF (FF/2F terminating class).

The ROM worker saves the negative opcode byte to +1 each
time, including the final FF. The C implementation now
retains 0xFF after terminal 0x2192, consistent with the
original. Updated ROM-free integration expectations and
strict original-ROM stepper differential; the latter will
be rerun to detect any remaining differences, never
waiving RAM comparisons.

This is a measured behavior correction, not a heuristic
audio replacement. No SCSP audibility is claimed.
