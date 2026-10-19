# v0879 — Continue original Motorola 68000 from engine entry to measured key-on/alternate gate

The optional --probe-engine mode has independently reached
the original program's playback engine at PC 0xC68 for both
AD-family 90 HIGH commands, in 87 original 68000 instructions
after PC 0xA92. Measured voice0+2=0x58 and voice0+3
is D6 (9B command) / 81 (9A command). These are original
CPU register/RAM execution results, not model predictions.

The opt-in diagnostic now reads the live sample table entry at
0x5000 + 20 * (voice0 select word & 0x7ff), tests the original
bit31 of word+0 and the nonzero long at +12, and chooses one
KNOWN executable boundary:
- 0xF10 if the bit31 key-on arm at 0xED8 is selected;
- 0xDF8 if secondary 0xD92 arm is selected;
- 0xCB2 for the fallback copy engine.

The original 68000 is stepped to that boundary and the SCSP
slot0 registers and stream byte at sound RAM 0x2000 are
reported. These are diagnostics and do not change the C
runtime or claim audible sound before actual results.
