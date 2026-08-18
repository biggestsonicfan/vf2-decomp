# Selector-3 phase-13 instruction-level recovery correction (v0.0.26)

Direct disassembly of the supported Virtua Fighter 2 Version 2.1 i960 program at `0x0000bdc8` closes a semantic error in the native phase-13 handler.

```text
0x0000bdc8  call      0x000094d0
0x0000bdcc  remo      11,g0,r14
0x0000bdd0  cmpobe    9,r14,0x0000bdc8
0x0000bdd4  call      0x000094d0
0x0000bdd8  remo      11,g0,r15
0x0000bddc  cmpobe    9,r15,0x0000bdd4
0x0000bde0  cmpobne   r14,r15,0x0000bde8
0x0000bde4  addo      13,r15,r15
0x0000bde8  ld        0x00500804,r4
0x0000bdf0  stob      r14,0x1b0(r4)
0x0000bdf4  ld        0x00500808,r4
0x0000bdfc  stob      r15,0x1b0(r4)
```

Both RNG results are reduced modulo 11 before use, remainder 9 is regenerated, and the `+13` adjustment applies only when the accepted remainders are equal because `cmpobne` skips the `addo` when they differ. The prior C retained the raw 16-bit RNG results, stored their low bytes and applied `+13` on raw inequality.

The recovered handler now reduces after every RNG call, retries remainder 9, compares the accepted remainders, offsets the second mode only on equality and stores the resulting mode bytes. A full live-state differential snapshot remains a useful separate checkpoint for the downstream phase-7 composition.
