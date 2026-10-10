# v0777: 0x1b780 slot3_table_lookup recovered

**Status**: 1-to-1 C translation. 4-leg ctest PASSES.

## Summary

Closed another tiny leaf in the v0775 (0x94d0 timer_fuse)
caller family. 0x1b780 is a 20-byte, 1-block, 4-instruction
function that calls 0x94d0 to get a 3-bit slot index, then
looks up a 16-bit word from a 8-entry main_data table.

This is the second of 2 known callers of v0775
(`0x542c8` was the first, recovered inside v0776's
`0x5427c poly_record_load`). Both callers are now
recovered; the timer_fuse sub-recovery closes cleanly.

## Disasm (1 block, with 3-gate fall-through)

```
block_0001b774 (3-gate header, fall-through to 0x1b780):
  0001b774  cmpobe   r3, r7, 0x0001b794  ; if r3 > r7 -> default
  0001b778  cmpobne  r3, r4, 0x0001b780  ; if r3 != r4 -> lookup
  0001b77c  cmpobl   r5, r6, 0x0001b794  ; if r5 < r6 -> default

block_0001b780 (lookup body, 4 instr):
  0001b780  call     0x000094d0          ; v0775 timer_fuse
  0001b784  and      7, g0, g0           ; g0 = g0 & 0x7 (3-bit slot)
  0001b788  ldos     0x0201cb8c[g0*2], g0 ; g0 = table[g0*2] (16-bit)
  0001b790  ret

block_0001b794 (default, 2 instr):
  0001b794  lda      0x00000282, g0      ; g0 = 0x282
  0001b798  ret
```

`vf2i960 function` reports the entry at `0x1b780` (the
explicit branch target of the `b 0x1b780` at `0x1b778`).
The 3 gates at `0x1b774..0x1b77c` form a fall-through header:
the original i960 code reaches `0x1b780` either via direct
branch or via fall-through when all 3 gates pass.

## Two paths

- **Lookup path** (all 3 gates "pass"):
  - `cmpobe r3, r7`: skip if `r3 <= r7` (no branch)
  - `cmpobne r3, r4`: branch to `0x1b780` if `r3 != r4`
  - `cmpobl r5, r6`: skip if `r5 >= r6` (no branch)
  - Body: call `0x94d0`, mask g0 to 3 bits, look up 16-bit
    word at `0x0201cb8c + slot*2`, return.

- **Default path** (any gate "fails"):
  - Return `0x282` constant.

## Recovery shape

Standard 1-to-1 C translation in `src/recovered/hybrid.c`.
Reads r3, r4, r5, r6, r7 from `cpu->registers[3..7]`.
The 3 gates are tested in i960-cmpob order, returning the
default value if any fails.

The lookup path calls `vf2_hybrid_timer_fuse_94d0_execute`
(v0775) with cpu->ip = 0x94d0. The recovered g0 is read
back from `cpu->registers[G0_REGISTER]`, masked to 3 bits,
then a `vf2_model2a_read` of 2 bytes at `0x0201cb8c +
slot*2` retrieves the 16-bit table word.

## Test

`tests/recovered/test_slot3_table_lookup_1b780_native.c`
(ctest #68). 4 legs:

- **Leg A**: lookup path. r3=0, r4=1, r5=2, r6=1, r7=0
  (gates pass). Timers t0=3, others=0 -> 0x94d0 returns 3.
  Set table[3] = 0xbeef; assert g0 = 0xbeef.
- **Leg B**: default via cmpobe. r3=5, r7=4 (r3 > r7).
  Assert g0 = 0x282.
- **Leg C**: default via cmpobne. r3=r4=7 (r3 == r4).
  Assert g0 = 0x282.
- **Leg D**: default via cmpobl. r3=1, r4=2 (cmpobne branch
  taken), r5=1, r6=2 (r5 < r6). Assert g0 = 0x282.

Timer setup: t0=3, t1=t2=t3=0 (for Leg A only) makes 0x94d0
return 3 (state = 0x30, return = 0x30>>4 = 3).

The 8-entry main_data table at 0x0201cb8c (offset 0x1cb8c
within main_data, 128 KB buffer covers it) is pre-filled per
leg with predictable values; table[3] is varied to test the
lookup result.

## cascade closure

With v0777 done, **all 2 known callers of v0775 (0x94d0)
timer_fuse are recovered**:

  - `0x1b780` v0777: slot3_table_lookup (this slice)
  - `0x542c8` v0776: alloc-path call inside poly_record_load
    (v0776's loop body)

The timer_fuse sub-recovery is now complete in both calling
contexts.

## Files added/modified

- `src/recovered/hybrid.c` - v0777 recovery appended at end
- `include/vf2/hybrid/player.h` -
  `vf2_hybrid_slot3_table_lookup_1b780_execute` declaration
- `decomp/i960/functions.csv` - row for 0x1b780 at line 110
- `CMakeLists.txt` - ctest #68 entry
- `tests/recovered/test_slot3_table_lookup_1b780_native.c` -
  4-leg test

ctest: 0.07 s, 4 legs PASS.