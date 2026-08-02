# References

Primary technical references for the project:

- MAME Sega Model 2 driver:
  `https://github.com/mamedev/mame/blob/master/src/mame/sega/model2.cpp`
- MAME Intel i960 implementation:
  `https://github.com/mamedev/mame/tree/master/src/devices/cpu/i960`
- Intel 80960KB Programmer's Reference Manual, document 270567-001.
- Intel i960 Processor Assembler User's Guide, document 272885.

MAME is used as executable hardware documentation and as a differential-testing
oracle. Its source is not copied into this repository. Intel documentation is
used to verify instruction and procedure-frame semantics.
