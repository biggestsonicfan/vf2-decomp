# v0778: 0x5a460 color_table_snapshot recovered

**Status**: 1-to-1 C translation. 4-leg ctest PASSES.

## Summary

Closed another small leaf in the 0x6de3c record-rebuild
dispatcher: a 176-byte bus-table snapshot at 0x5a460 that
reads 7 bytes from a bus-relative address window and stores
them to work_ram in a "shuffled" snapshot order.

The function is the sub-call of `0x6010c` (which we
**deliberately did not recover** — it has 134 byte stores
that are essentially the same shape as v0774 and would add
no new ground; v0778 closes the hole left in 0x6de3c's
call graph that v0774 couldn't reach).

## Disasm (1 block, 26 instr)

```
block_0005a460:
  0005a460  ld       0x0050016c, r3           ; r3 = bus_ptr
  0005a468  ldob     0x00003356(r3), r3       ; r3 = bus[0x3356]
  0005a470  ld       0x0050016c, r4
  0005a478  ldob     0x00003359(r4), r4       ; r4 = bus[0x3359]
  0005a480  ld       0x0050016c, r5
  0005a488  ldob     0x00003357(r5), r5       ; r5 = bus[0x3357]
  0005a490  ld       0x0050016c, r6
  0005a498  ldob     0x0000335a(r6), r6       ; r6 = bus[0x335a]
  0005a4a0  ld       0x0050016c, r7
  0005a4a8  ldob     0x00003358(r7), r7       ; r7 = bus[0x3358]
  0005a4b0  ld       0x0050016c, r8
  0005a4b8  ldob     0x0000335b(r8), r8       ; r8 = bus[0x335b]
  0005a4c0  ld       0x0050016c, r9
  0005a4c8  ldob     0x0000335c(r9), r9       ; r9 = bus[0x335c]
  0005a4d0  stob     r3, 0x00500234           ; dst[0x234] = bus[0x3356]
  0005a4d8  stob     r4, 0x00500235           ; dst[0x235] = bus[0x3359]
  0005a4e0  stob     r5, 0x00500236           ; dst[0x236] = bus[0x3357]
  0005a4e8  stob     r6, 0x00500237           ; dst[0x237] = bus[0x335a]
  0005a4f0  stob     r7, 0x00500238           ; dst[0x238] = bus[0x3358]
  0005a4f8  stob     r8, 0x00500239           ; dst[0x239] = bus[0x335b]
  0005a500  stob     r9, 0x0050023a           ; dst[0x23a] = bus[0x335c]
  0005a508  call     0x00002b4c               ; v0x2b4c sub-call
  0005a50c  ret
```

The 7 source offsets are **NOT consecutive**: 0x3356, 0x3359,
0x3357, 0x335a, 0x3358, 0x335b, 0x335c. They form a
"shuffled" pattern over the 7-byte window 0x3356..0x335c:

  - r3 reads from 0x3356 (1st byte)
  - r4 reads from 0x3359 (4th byte)
  - r5 reads from 0x3357 (2nd byte)
  - r6 reads from 0x335a (5th byte)
  - r7 reads from 0x3358 (3rd byte)
  - r8 reads from 0x335b (6th byte)
  - r9 reads from 0x335c (7th byte)

This is likely a reordering for some downstream color-channel
interpolation; the destination 0x500234..0x50023a receives
the bytes in the same shuffled order.

## Recovery shape

Standard 1-to-1 C translation in `src/recovered/hybrid.c`.
Reads `bus_ptr = *(0x50016c)` (a u32), then reads 7 bytes
from the bus-relative offsets in the same shuffled order,
then writes them to 0x500234..0x50023a.

After the 7 stores, the recovery **REFUSES** the sub-call
to `0x2b4c` (color_table_rebuild, deferred per v0751) by
returning `VF2_ERROR_UNSUPPORTED` with `cpu->ip` set to
the post-call address `0x5a510`. The 7 byte stores happen
BEFORE the refusal; if the dispatcher wires the recovery
up it can fall back to interpretation for the `0x2b4c`
sub-call.

## Test

`tests/recovered/test_color_table_snapshot_5a460_native.c`
(ctest #69). 4 legs:

- **Leg A**: identity bytes 0..6 at bus+0x3356..0x335c ->
  verify dst bytes are in the shuffled snapshot order
  `(0, 3, 1, 4, 2, 5, 6)`.
- **Leg B**: 0xAA repeating -> all dst bytes = 0xAA.
- **Leg C**: distinct bytes 10..16 -> dst = (10, 13, 11, 14,
  12, 15, 16) in shuffled order.
- **Leg D**: different `bus_ptr` value (0x0050a000) to verify
  the address is read from 0x50016c each call.

The test sets bus_ptr, fills the 7 source bytes at the
shuffled offsets, then calls the recovery. After the
recovery (which returns VF2_ERROR_UNSUPPORTED), the test
checks that the 7 dst bytes match the snapshot order.

## cascade closure

With v0778 done, the 0x6de3c record-rebuild dispatcher's
sub-call graph is closing:
  - 0x9480 v0771 (CRC/hash)        - closed
  - 0x5ff7c v0772 (block_hash)      - closed
  - 0x6d0a0 v0773 (frame_end_tick)  - closed
  - 0x6010c (poly_record_fill)      - NOT YET recovered
                                      (134 byte stores, no
                                      new ground vs v0774)
  - 0x60330 v0774 (poly_record_clear) - closed
  - 0x5427c v0776 (poly_record_load) - closed (calls v0775)
  - 0x94d0 v0775 (timer_fuse)       - closed
  - 0x1b780 v0777 (slot3_table_lookup) - closed
  - 0x5a460 v0778 (color_table_snapshot) - closed (this slice)
  - 0x2b4c (color_table_rebuild)    - deferred per v0751
                                      (complex mulo/divo)
  - 0x6de3c (the dispatcher itself) - NOT YET recovered

The dispatcher is large (912 B / 9 blocks / 56 instr) and
ties all these together; recovering it is a follow-up slice.

## Files added/modified

- `src/recovered/hybrid.c` - v0778 recovery appended at end
- `include/vf2/hybrid/player.h` -
  `vf2_hybrid_color_table_snapshot_5a460_execute` declaration
- `decomp/i960/functions.csv` - row for 0x5a460
- `CMakeLists.txt` - ctest #69 entry
- `tests/recovered/test_color_table_snapshot_5a460_native.c` -
  4-leg test

ctest: 0.04 s, 4 legs PASS.