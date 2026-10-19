# v0880 — Correct MC68000 MOVEM.L D0-D7,(A2) legality at the VF2 sound engine

Original sound CPU ROM executes 0x48D2 0x00FF at 0xCB6 and
0x23FA. Prior tools incorrectly declared the (A2) destination
illegal / vector-4. Motorola's M68000 Family Programmer's
Reference Manual, instruction MOVEM, **explicitly lists
address-register indirect (An) as a LEGAL register-to-memory
control-alterable addressing mode**. Only postincrement
(An)+ is prohibited in this direction; predecrement -(An)
is also legal.

Authoritative ISA source:
https://www.nxp.com/docs/en/reference-manual/M68000PRM.pdf
MOVEM instruction addressing mode table, pages 4-129/4-130.

Implementation:
- step_movem now accepts mode 2 (An) register->memory.
- the register->memory control-mode order is corrected to
  ascending D0..D7, then A0..A7 (predecrement is untouched).
- decoder now identifies 0x48D2 as `movem.l $00ff,(a2)`
  instead of an unsupported .word.
- test_stepper exercises a full D0-D7 longword copy, unchanged
  address register and OOB refusal; test_decoder pins opcode.

This is a PROCESSOR ISA CORRECTION based on Motorola's manual,
not a claim that the game produced an audible sound. Re-evaluate
the original VF2 ROM at 0xCB6: if the *destination address*
is unmapped, bus access may still legitimately fail. Do not
interpret opcode legality as evidence of mapped sound RAM or
successful SCSP key-on; observe actual bus/stepper result.

No game ROMs, dumps, or audio are included in this commit.
