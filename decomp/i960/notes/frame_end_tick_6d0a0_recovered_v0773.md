# v0773 — `sub_00006d0a0` `frame_end_tick` recovered

Recovered the 27-instruction, 4-block, 108-byte tick
function at `0x6d0a0..0x6d10c` (one past the `ret` at
`0x6d108`). It calls the v0769 queue dedup-append
(`0x6d1b4`) and the v0772 block_hash wrapper (`0x5ff7c`),
with a `base+0x33fc` counter increment (mirrored to
`0x01d033fc` and `0x0059c3fc`) and a low-16-bit bus pointer
store to `0x5000e4`.

## Disassembly (verified via `vf2i960 disasm ../../roms/vf2 0x6d0a0 32`)

```
0006d0a0  5c80160e  mov      r14, g0            ; g0 = r14 (local)
0006d0a4  09000110  call     0x0006d1b4         ; v0769 sub-call
0006d0a8  90807400  ld       0xfffffffc(sp), g0 ; g0 = sp-4 scratch
0006d0b0  59084904  subo     4, sp, sp          ; (canceled)
0006d0b4  59084804  addo     4, sp, sp          ;
0006d0b8  92787400  st       r15, 0xfffffffc(sp); r15 -> sp-4 scratch
0006d0c0  90783000  ld       0x0050016c, r15    ; r15 = bus ptr
0006d0c8  907bf400  ld       0x000033fc(r15), r15; r15 = bus[0x33fc]
0006d0d0  597bc801  addo     1, r15, r15        ; r15++
0006d0d4  92783000  st       r15, 0x01d033fc    ; backup_sram mirror
0006d0dc  92783000  st       r15, 0x0059c3fc    ; extended mirror
0006d0e4  90787400  ld       0xfffffffc(sp), r15; r15 <- sp-4 (restore)
0006d0ec  59084904  subo     4, sp, sp          ;
0006d0f0  8c783000  lda      0x0000ffff, r15    ; r15 = 0xffff mask
0006d0f8  581bc083  and      r3, r15, r3        ; r3 (bus ptr) &= 0xffff
0006d0fc  92183000  st       r3, 0x005000e4     ; r3 (low16) -> work_ram
0006d104  09ff2e78  call     0x0005ff7c         ; v0772 sub-call
0006d108  0a000000  ret
```

- `r3` is the bus pointer in this function (set by v0769
  from `*(0x50016c)`); r3 is **NOT** the i960 stack pointer
  (r1 is, see below).
- The `ld/st 0xfffffffc(sp)` dance is a scratch-word
  save/restore of r15 across the body. The original code
  reads/writes it from the caller's local storage; the
  test must set r1 (SP) to a work_ram address.

## Recovery (`src/recovered/hybrid.c` end)

`vf2_hybrid_frame_end_tick_6d0a0_execute` is a 1-to-1 C
translation. It dispatches the sub-calls via the v0775g
pattern: temporarily set `cpu->ip`, call the sub-recovery,
overwrite `cpu->ip` for the post-ret address.

## Test (`tests/recovered/test_frame_end_tick_6d0a0_native.c`)

2 legs, each:

1. Set `0x50016c` to a bus pointer.
2. Set `bus+0x33fc` to a counter value, `bus+0x393c+80` to
   a queue value.
3. Upload the hash table to attached `main_data_buf`.
4. Write input bytes to `INPUT_BASE = bus+0x33a8`.
5. Set `cpu->registers[1] = SP_ADDR` (work_ram) so the
   sp-4 scratch stays in work_ram.
6. Run the recovery.
7. Assert mirrors and the `0x5000e4` store.

| leg | bus | initial_bump | initial_queue | expected mirrors | low16 |
|---|---|---|---|---|---|
| A | 0x4fd458 | 100 | 200 | (post_input+1) | 0xd458 |
| B | 0x4fe458 | 1 | 2 | (post_input+1) | 0xe458 |

ctest entry, 0.04 s, PASS.

## What I learned while writing the test

### i960 SP is r1, not r3

In the i960 KBx architecture, **r1 is the stack pointer**
(SP), not r3. r3 is a general-purpose local register. My
first cut of the recovery used `cpu->registers[3u]` for
the `ld/st sp-4` operations and got garbage. The fix is to
use `cpu->registers[1u]`. This was a real bug in the
recovery; the existing v0771/v0772 recoveries don't have
SP-relative operations so the bug didn't surface.

### v0769 sub-recovery does NOT preserve its result in cpu->registers[3]

The v0769 `sibling_slot_increment_helper_6d1b4` reads
`*(0x50016c)` into a LOCAL `r3` variable and uses it for
the address calculation, but never writes the local back
to `cpu->registers[3]`. After the sub-recovery returns,
`cpu->registers[3]` is whatever it was before (typically 0
from the frame memset in `enter_procedure`).

The `and r3, r15, r3` in 0x6d0a0 needs the bus pointer in
r3 to mask it. The fix is for v0773 to re-read `0x50016c`
into `cpu->registers[3]` after the 0x6d1b4 sub-call returns.

This is a small wart in the v0769 contract: callers that
need the bus-pointer-in-r3 from v0769 must re-read 0x50016c
themselves. A future v0773+ could fix the v0769 recovery to
write `cpu->registers[3u] = r3;` at the end, but that would
modify a previously committed recovery.

### Disasm is mirror-only for `0x6d0a0` AND `0x6d1b4`

The `0x33fc` counter increment in `0x6d0a0` does NOT write
back to `bus+0x33fc` — it only mirrors to `0x01d033fc` and
`0x0059c3fc`. Similarly, the queue dedup-append in `0x6d1b4`
does NOT write back to `bus+0x393c+slot*4` — only mirrors.

The v0768 `slot_increment_helper_6d10c` and v0769
`sibling_slot_increment_helper_6d1b4` write to the mirrors
*and* the source. But `0x6d0a0`'s 0x33fc increment is mirror
only. Reading more carefully: the v0768/v0769 rows in
`functions.csv` say "increment slot and mirror"; the 0x6d0a0
body says "increment and mirror" (no source write). The
distinction is subtle but matters for test setup.

### `0x5000e4` lies inside the hash input range

The 0x5ff7c sub-recovery reads input bytes from
`bus+0x33a8..bus+0x33a8+0x644`. The 0x6d0a0 body writes
`(bus & 0xffff)` to the FIXED address `0x5000e4`. If the
input is set up at `INPUT_BASE = bus+0x33a8 = 0x500800`,
then `0x5000e4 = INPUT_BASE + 0xe4` lies inside the input
range. The 0x6d0a0 body writes to `0x5000e4` BEFORE
calling 0x5ff7c, so the hash sees a modified input
(`input[0xe4..0xe7]` is overwritten with the low-16 bus
ptr bytes).

The test's `naive_hash` operates on the test's pristine
input, not the post-recovery-modified input. So the hash
result assertion cannot be made against a naive_hash. The
v0773 ctest therefore skips the hash assertion and relies
on the v0772 ctest for hash end-to-end coverage.

The ctest instead asserts the `0x5000e4` value (low 16 of
bus ptr) and the mirrors. This is the cleanest test that
verifies the unique behaviour of 0x6d0a0 without re-deriving
the full post-recovery input trace.

## `functions.csv` row

Added at line 24 (right after the v0772 row):

```
0x0006d0a0,0x0006d10c,frame_end_tick,recovered-rom-anchor,rom-disassembly+unit,
27 instructions; calls v0769 (0x6d1b4 queue dedup-append) and
v0772 (0x5ff7c block_hash) with a base+0x33fc counter
increment mirrored to 0x01d033fc+0x0059c3fc and a low-16-bits
bus pointer store to 0x5000e4; recovered v0773; extent
0x6d0a0..0x6d10c (one past the entry ret at 0x6d108),
```

`end` is `0x6d10c` (one past `ret` at `0x6d108`); the v0734a
two-source rule is satisfied (vf2i960 function reports the
same extent, and `vf2i960 disasm` confirms `0x6d108` is
`0a000000 ret`).

## What is NOT yet done

- `0x6d0a0` is a standalone execute hook, not yet wired
  into the dispatcher chain. Per the v0755 / v0770 cascade
  map, wiring stays deferred.
- The next cascade step per `study_deal_v0770.md` is the
  gap-run siblings: `sub_00006cf00` (524 B) and
  `sub_00006cc00` (620 B). Both have the same pattern as
  v0773 (sub-call chain, mirror writes, fixed-address
  stores) but with 64-bit carry arithmetic, multiplication
  and indirect table accesses — at least a multi-iteration
  autonomous slice.
- This slice is `src/` delta of one new function, one new
  ctest entry, one new CSV row. No `src/` change to
  existing recoveries (the v0769 sub-call still doesn't
  preserve r3, but v0773 works around it).
