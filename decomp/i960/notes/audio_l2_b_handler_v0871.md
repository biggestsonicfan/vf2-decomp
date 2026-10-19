# v0871 — AD23/AD24 L2 stream bytes select the original B handler

Original VF2.1 68000 code first initializes a 0x20-byte descriptor
from the A0 class (v0869) and the L2 worker decrements the descriptor
count from 1 to 0 at 0x1f88 (v0870). The pointer stored in +4 of
the descriptor is the original ROM record address +1.

ROM-anchored continuation:
- 0x1f92 loads A3 = descriptor[+4].
- 0x1f96 clears descriptor status bit0.
- 0x1f9c reads the next stream byte (A3)+.
- 0x1f9e BMI to 0x1fa8 for negative bytes.
- 0x1fa8 files that byte at descriptor[+1].
- 0x1fac..0x1fb4 extracts (byte & 0xf0) >> 2.
- 0x1fb6..0x1fba dispatches through the 16-entry jump table
  starting at 0x1fbc.
- Both AD23 byte 0xbb and AD24 byte 0xba choose offset 0x2c:
  table entry 0x1fe8, BRA to handler 0x1ffc.

User ROM examples:
AD2342 -> record 0x60b4fe, stream 0xbb, next A3 0x60b500.
AD2401 -> record 0x60b7f2, stream 0xba, next A3 0x60b7f4.

The new audio_l2_b_route.py verifies all 21 instruction/extension
words and returns the exact selected target, without attempting to
run a Motorola 68000 or synthesize SCSP audio. Five tests passed
locally, including real-ROM checks for both family examples and
mutated opcode/table refusals.

Next substantive implementation target: the original B0 sequence
applier at 0x1ffc. Its behavior must be recovered and
differential-tested before the existing C L2 preflight can safely
process its pending countdown-1 descriptor. No live gameplay SFX
or hardware audio is claimed in this static slice.
