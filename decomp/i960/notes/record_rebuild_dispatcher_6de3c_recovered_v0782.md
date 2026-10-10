# v0782 - 0x6de3c record_rebuild_dispatcher recovered

## Scope

Recovered `0x6de3c` (912 B / 9 blocks / 56 instr) as a 1-to-1
C translation. **This closes the v0770 cascade** — every
sub-call (`0x9480` v0771, `0x6010c` v0781, `0x5427c` v0776,
`0x60330` v0774) is now recovered, and the dispatcher itself
is recovered as the top-level wrapper that ties them
together.

The dispatcher:
1. Calls `0x9480` (v0771) with caller-supplied `g0/g1/g2` →
   `g0 = hash1` of the assignment block.
2. Compares `hash1` to the stored value at `0x01d03302`. If
   they differ, rebuilds:
   - call `0x6010c` (v0781) — writes hardcoded defaults
   - call `0x5427c` (v0776) — mirrors the block
   - recomputes the hash of `bus + 0x3340` (29 bytes), stores
     at `0x01d03302`
3. Computes `hash2` of `bus + 0x3320` (15 bytes) via `0x9480`.
4. Compares `hash2` to the stored value at `0x01d03300`. If
   they differ, rebuilds:
   - call `0x60330` (v0774) — clears the poly record
   - call `0x5427c` (v0776) — mirrors the block
   - recomputes the hash of `bus + 0x3320` (15 bytes), stores
     at `0x01d03300`
5. Sets `bus_ptr = 0x01d00000`, sets bit 0 of `0x500171`.
6. Mirror-copies `0x3fe0` bytes from `backup_sram
   0x01d00000` to `extended_work_ram 0x00599000` via
   `ldq/stq` with `cmpdeci + bl` loop.
7. Writes the "VIRTUA FIGHTER 2" signature (count = 24 +
   4 little-endian dwords) at `bus + 0x3306..0x3317`. The
   count uses a 16-bit store (`stos`), not a 32-bit store,
   to avoid clobbering the first 2 bytes of the "VIRT"
   dword at `0x3308`.
8. Sets `bus_ptr = 0x00599000`.
9. Zeros `0x500148..0x50014b` (4 bytes).
10. `ret`.

## Sub-calls (all recovered, all IP-set before invocation)

- `vf2_hybrid_crc16_step_hash_9480_execute` ×3
- `vf2_hybrid_assignment_init_6010c_execute` ×1
- `vf2_hybrid_poly_record_load_5427c_execute` ×2
- `vf2_hybrid_poly_record_clear_60330_execute` ×1

## Disassembly

```
0006de3c  call     0x00009480
0006de40  ldos     0x01d03302, r15
0006de48  cmpobe   g0, r15, 0x0006de78
0006de4c  call     0x0006010c
0006de50  call     0x0005427c
0006de54  ld       0x0050016c, r15
0006de5c  lda      0x00003340(r15), g0
0006de64  mov      29, g2
0006de68  mov      0, g1
0006de6c  call     0x00009480
0006de70  stos     g0, 0x01d03302
0006de78  ld       0x0050016c, r15
0006de80  lda      0x00003320(r15), g0
0006de88  mov      15, g2
0006de8c  mov      0, g1
0006de90  call     0x00009480
0006de94  ldos     0x01d03300, r15
0006de9c  cmpobe   g0, r15, 0x0006decc
0006dea0  call     0x00060330
0006dea4  call     0x0005427c
0006dea8  ld       0x0050016c, r15
0006deb0  lda      0x00003320(r15), g0
0006deb8  mov      15, g2
0006debc  mov      0, g1
0006dec0  call     0x00009480
0006dec4  stos     g0, 0x01d03300
0006decc  b        0x0006df6c
0006df6c  shlo     20, 29, r15
0006df70  st       r15, 0x0050016c
0006df78  ldob     0x00500171, r3
0006df80  setbit   0, r3, r3
0006df84  stob     r3, 0x00500171
0006df8c  lda      0x01d00000, r3
0006df94  lda      0x00599000, r4
0006df9c  lda      0x00003fe0, r5
0006dfa4  shro     4, r5, r5
0006dfa8  ldq      (r3), r8
0006dfac  addo     16, r3, r3
0006dfb0  stq      r8, (r4)
0006dfb4  addo     16, r4, r4
0006dfb8  cmpdeci  1, r5, r5
0006dfbc  bl       0x0006dfa8
0006dfc0  b        0x0006e140
0006e140  ld       0x0050016c, r8
0006e148  lda      0x54524956, r15   ; "VIRT"
0006e150  st       r15, 0x00003308(r8)
0006e158  lda      0x46204155, r15   ; "UA F"
0006e160  st       r15, 0x0000330c(r8)
0006e168  lda      0x54484749, r15   ; "IGHT"
0006e170  st       r15, 0x00003310(r8)
0006e178  lda      0x32205245, r15   ; "ER 2"
0006e180  st       r15, 0x00003314(r8)
0006e188  mov      24, r4
0006e18c  stos     r4, 0x00003306(r8) ; 16-bit count
0006e194  lda      0x00599000, r15
0006e19c  st       r15, 0x0050016c
0006e1a4  mov      0, r3
0006e1a8  stob     r3, 0x00500148
0006e1b0  stob     r3, 0x00500149
0006e1b8  stob     r3, 0x0050014a
0006e1c0  stob     r3, 0x0050014b
0006e1c8  ret
```

## Bugs caught and fixed during v0782

1. **stos vs st** for the count at `0x3306` — the
   disassembly uses `stos` (16-bit store), not `st`
   (32-bit store). The first commit used a 4-byte write
   which clobbered the first 2 bytes of the "VIRT"
   dword at `0x3308`. Fixed by using `uint16_t` with
   `sizeof = 2`.
2. **ldos reads into r15** — the `ldos` instruction
   is a 16-bit zero-extended load. Reading via
   `vf2_model2a_read(..., &r15, 2)` only writes the
   low 2 bytes of `r15`; the high 2 bytes are
   uninitialised. The `cmpobe g0, r15` then compares
   a stale high half. Fixed by reading into a
   `uint16_t` first, then assigning `(uint32_t)stored`
   to `r15`.
3. **CRC table required in main_data** — the v0771
   hash reads a 256-entry 16-bit table at
   `main_data 0x02000000`. The test pre-fills
   `main_data[0..0x1ff]` with the
   `fill_table_default` pattern (matching the v0771
   test). Without this, the hash returns 0 and
   the dispatcher's "stored vs computed" comparison
   misbehaves.
4. **v0776 poly_record_load sub-call inputs** — the
   dispatcher calls `0x5427c` (v0776) which needs
   timers at `0xf00000+0..12`, the polygon table
   at `main_data 0x0201eafc`, and input data at
   `main_data 0x0201ea24`. The test pre-fills all
   three so the rebuild path runs cleanly.

## Test

ctest #73: `vf2_record_rebuild_dispatcher_6de3c`
(0.02 s) PASSES. Four legs:
- A: both hashes match → no rebuild → only copy +
  signature + reset
- B: hash1 mismatch → assignment rebuild
- C: hash2 mismatch → poly record rebuild
- D: both mismatch → both rebuilds

Each leg verifies: status==OK, ip==post-ret, stored
hashes updated to true values, bus_ptr reset to
`0x599000`, mirror copy correct, flag bit 0 set,
signature at `0x1d03306..0x1d03317` (4 dwords +
count), zero region `0x500148..0x50014b` zeroed,
and (when applicable) the v0774/v0781 sub-calls
produced the expected record bytes at
`0x59c320..0x59c35c`.

## File map

- `src/recovered/hybrid.c`: function
  `hybrid_execute_record_rebuild_dispatcher_6de3c` +
  wrapper.
- `include/vf2/hybrid/player.h`: declaration.
- `decomp/i960/functions.csv`: row added for
  `0x6de3c..0x6e1cc record_rebuild_dispatcher`.
- `tests/recovered/test_record_rebuild_dispatcher_6de3c_native.c`:
  4-leg unit test.
- `CMakeLists.txt`: ctest #73.

## Cascade closure

With v0782 committed, the **v0770 cascade is fully
closed**. The 0x9480 → 0x5ff7c → 0x6d0a0 chain
(v0771, v0772, v0773), the 0x6cf00 sibling (v0779),
and the 0x6de3c record-rebuild dispatcher (v0782)
are all in place, with every sub-call recovered and
all measurable behaviour pinned at the 0x6de3c
entry boundary.
