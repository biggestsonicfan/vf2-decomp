# v0779: 0x6cf00 frame_end_tick_33f0 recovered

**Status**: 1-to-1 C translation. 4-leg ctest PASSES.

## Summary

Closed the second frame-end-tick leaf in the v0772
(0x5ff7c block_hash_apply) cascade. 0x6cf00 is a 524 B /
2 blocks / 13 instr sibling of v0773 (frame_end_tick 0x6d0a0)
that increments a DIFFERENT counter offset (bus+0x33f0 vs
v0773's bus+0x33fc) before calling v0772.

The disasm shows this function has its own sp-4 preamble
(ld sp-4 / subo 4, sp / b 0x6d0f0) that jumps into a
**shared epilogue** at 0x6d0f0 — the same epilogue used by
v0773's tail (mask r3 to 16 bits, store to 0x5000e4, call
v0772). The "main_post_timer" function at 0xa038 also uses
this epilogue, so 0x6d0f0 is a shared "block_hash + low16
bus ptr" epilogue.

## Disasm (2 blocks, 13 instr)

```
block_0006cf00 (counter inc + sp prep, 7 instr):
  0006cf00  ld       0x0050016c, r15      ; r15 = bus_ptr
  0006cf08  ld       0x000033f0(r15), r15  ; r15 = bus[0x33f0]
  0006cf10  addo     1, r15, r15          ; r15 += 1
  0006cf14  st       r15, 0x01d033f0      ; backup_sram mirror
  0006cf1c  st       r15, 0x0059c3f0      ; extended work_ram mirror
  0006cf24  ld       0xfffffffc(sp), r15  ; r15 = sp-4 scratch
  0006cf2c  subo     4, sp, sp
  0006cf30  b        0x0006d0f0           ; -> shared epilogue

block_0006d0f0 (shared epilogue, 6 instr):
  0006d0f0  lda      0x0000ffff, r15      ; r15 = 0xffff mask
  0006d0f8  and      r3, r15, r3          ; r3 &= 0xffff (low 16)
  0006d0fc  st       r3, 0x005000e4      ; store low-16 bus ptr
  0006d104  call     0x0005ff7c          ; v0772 block_hash_apply
  0006d108  ret
```

## Differences from v0773 (frame_end_tick 0x6d0a0)

- **Counter offset**: v0779 uses bus+0x33f0, v0773 uses
  bus+0x33fc. Both are 32-bit slot counters in the bus
  pointer's table.
- **Epilogue**: v0779 jumps to a SHARED epilogue at 0x6d0f0
  (block_0006d0f0). v0773 has its own epilogue inline.
- **No queue sub-call**: v0773 calls v0769 (queue
  dedup-append) before the counter inc; v0779 does not.
  v0779 is a "counter + hash" only variant.

## Mirror-only writes

The disasm stores the incremented value ONLY to:
- 0x01d033f0 (backup_sram mirror)
- 0x0059c3f0 (extended work_ram mirror)

It does NOT write back to bus[0x33f0]. This is the same
"mirror-only" pattern as v0773 and v0776's queue-slot
writes (per v0773 / v0776 slice notes). The mirrors are
read elsewhere; the bus source is left unchanged.

## Recovery shape

Standard 1-to-1 C translation in `src/recovered/hybrid.c`.
Reads `bus_ptr = *(0x50016c)`, reads `bus[0x33f0]`,
increments, stores to mirrors 0x01d033f0 and 0x0059c3f0.
Saves `r15 = *(sp-4)` (i960 SP=r1 NOT r3), adjusts sp.
Jumps to the shared epilogue at 0x6d0f0: masks r3 to
16 bits and stores to 0x5000e4, then calls v0772.

Sub-call to v0772 uses the recovered
`vf2_hybrid_block_hash_apply_5ff7c_execute`.

## Test

`tests/recovered/test_frame_end_tick_33f0_6cf00_native.c`
(ctest #70). 4 legs:

- **Leg A**: bus_ptr=0x500800, counter=0, r3=0x12345678 ->
  mirrors = 1, 0x5000e4 = 0x5678.
- **Leg B**: bus_ptr=0x500800, counter=100, r3=0xaabbccdd
  -> mirrors = 101, 0x5000e4 = 0xccdd.
- **Leg C**: bus_ptr=0x500800, counter=0xffffffff, r3=0x5
  -> mirrors = 0 (32-bit wrap), 0x5000e4 = 0x0005.
- **Leg D**: bus_ptr=0x0050a000, r3=0xfeedface -> mirrors
  = 1, 0x5000e4 = 0xface.

The v0772 sub-recovery transitively calls v0771 which
reads a 16-bit table from main_data 0x02000000, so we
attach a 128 KB main_data buffer pre-filled with a known
identity table.

## Issues found and resolved

1. **enter_procedure zeros registers**: setting
   `cpu->registers[3] = r3_value` BEFORE
   `vf2_i960_cpu_enter_procedure` is incorrect because
   enter_procedure zeros the registers. Fix: set r3 AFTER
   enter_procedure.

2. **Mirror-only writes**: the original code stores ONLY
   to the mirror addresses (0x01d033f0, 0x0059c3f0), not
   back to bus[0x33f0]. Initial test checked bus[0x33f0]
   and failed; fix: check mirrors only.

## cascade closure

With v0779 done, both frame-end-tick leaves that call v0772
are recovered:
- 0x6d0a0 (v0773): bus+0x33fc counter
- 0x6cf00 (v0779): bus+0x33f0 counter

The shared epilogue at 0x6d0f0 is also used by 0xa038
(main_post_timer) but main_post_timer has a separate
extent and isn't directly recovered yet (it's the
corridor-end main-loop tail, deferred).

## Files added/modified

- `src/recovered/hybrid.c` - v0779 recovery appended at end
- `include/vf2/hybrid/player.h` -
  `vf2_hybrid_frame_end_tick_33f0_6cf00_execute` declaration
- `decomp/i960/functions.csv` - row for 0x6cf00
- `CMakeLists.txt` - ctest #70 entry
- `tests/recovered/test_frame_end_tick_33f0_6cf00_native.c` -
  4-leg test

ctest: 0.05 s, 4 legs PASS.