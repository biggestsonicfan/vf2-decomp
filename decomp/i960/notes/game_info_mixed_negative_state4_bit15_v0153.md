# v0153: mixed negative state-4 bit-15 recovery

ROM-backed probes show that mixed state-4 pairs at a negative shared threshold with combined `fighter + 0x1a4` flags equal to bit 15 can use the already recovered state-4 bit-15 C path instead of the interpreter fallback.

The native body already matched memory and instruction accounting. The remaining architectural difference was the final i960 condition state: the ROM leaves `compare_result = NONE` while the recovered path left `GREATER`.

Validation covered 160 snapshot pairs across state-4 on either fighter, bit-15 distribution on either/both fighters, countdown 0/1, mode-bit6 off/on, and counterpart states 0,1,2,3,5,6,7,8,9,10,15,255. All 160 final snapshots matched exactly.
