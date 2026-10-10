# v0776: 0x5427c poly_record_load recovered

**Status**: 1-to-1 C translation. 4-leg ctest PASSES.

## Summary

Closed the third leaf in the `0x9480`-cascade sequence started in v0771:
the polygon record loader at `0x5427c` (156 B / 8 blocks / 31 instructions).

The function is the second of three sub-calls invoked by `0x6de3c`'s
record-rebuild sequence (after `0x60330` v0774 and before `0x94d0` v0775):

  - `0x6de3c` (912 B) - per-frame record-rebuild dispatcher (not recovered yet)
  - `0x60330` v0774 - 220 B record-clear at 0x01d03320..0x01d0332e
  - `0x5427c` v0776 - **this slice** - 156 B 18-iter record loader
  - `0x94d0` v0775 - 88 B 4-timer fuse allocator (sub-call of 0x5427c)

Two callers (per `vf2i960 xrefs`): `0x6de50`, `0x6dea4`. Both inside
`0x6de3c`.

## Disasm (8 blocks / 31 instr)

```
block_0005427c (init, 8 instr):
  0005427c  lda      0x01d03100, r3      ; r3 = dst base
  00054284  lda      0x0201ea24, r4      ; r4 = src base
  0005428c  mov      0, r5               ; r5 = 0
  00054290  mov      0, r6               ; r6 = slot bitmap (32 bits)
  00054294  mov      0, r8               ; r8 = src byte index
  00054298  mov      0, r10              ; r10 = iter counter
  0005429c  mov      0, r14              ; r14 = 0 (zero byte)
  000542a0  lda      0x1ff, r15          ; r15 = 0x1ff

block_000542a4 (zero-fill loop, 3 instr, 256 iters):
  000542a4  stob     r14, (r3)[r15]      ; dst[r15] = 0
  000542a8  cmpdeco  0, r15, r15         ; r15 = r15-1 if r15>0 else 0xffffffff
  000542ac  bl       0x000542a4          ; branch back if signed(r15) > 0
                                          ; (i.e. CC == LESS)

block_000542b0 (main loop top, 3 instr):
  000542b0  muli     16, r10, r9         ; r9 = r10 * 16 (struct stride)
  000542b4  cmpobl   6, r10, 0x542c8     ; if r10 < 6 -> alloc path
  000542b8  setbit   r10, r6, r6         ; (prealloc path) r6 |= 1<<r10
  000542bc  ld       0x0201eafc[r10*4], r11
  000542c4  b        0x000542e0          ; -> common body

block_000542c8 (alloc path, 3 instr):
  000542c8  call     0x000094d0          ; v0775 timer_fuse
  000542cc  and      31, g0, r5          ; r5 = g0 & 0x1f (5-bit slot)
  000542d0  bbs      r5, r6, 0x542c8     ; retry while bit r5 set in r6

block_000542d4 (alloc done, 2 instr):
  000542d4  setbit   r5, r6, r6          ; r6 |= 1<<r5
  000542d8  ld       0x0201eafc[r5*4], r11

block_000542e0 (common body, 9 instr):
  000542e0  ld       (r11), r11          ; r11 = *r11 (deref polygon ptr)
  000542e4  st       r11, 0x4(r3)[r9]    ; dst[r9+4] = r11
  000542ec  ld       (r4)[r8], r11       ; r11 = *(src + r8) (4B)
  000542f0  st       r11, (r3)[r9]       ; dst[r9+0] = r11
  000542f8  ldob     0x6(r4)[r8], r11    ; r11 = *(byte)(src + r8 + 6)
  00054300  stob     r11, 0xa(r3)[r9]    ; dst[r9+10] = r11
  00054308  cmpinco  17, r10, r10        ; r10 = r10+1 if r10<=17 else 17
  0005430c  addo     12, r8, r8          ; r8 += 12 (src stride)
  00054310  bg       0x000542b0          ; branch back if r10 > 17

block_00054314: ret
```

## Behaviour

1. **256-byte zero-fill** (block_000542a4): write 0 to dst[0x1ff]
   down to dst[0]. Net result: 16 sprite descriptor slots at 16 B
   each, all zero.

2. **18-iteration main loop** (r10 = 0..17):
   - **r10 < 6 (alloc path)**: call v0775 `0x94d0` to allocate a
     free 5-bit slot. The `bbs` retry ensures a unique slot.
   - **r10 >= 6 (prealloc path)**: use r10 directly as slot index.
   - **Common body**: write 9 bytes to dst+16*r10 at offsets
     0 (4B from src+r8), 4 (4B deref of polygon_table[slot]),
     and 10 (1B from src+r8+6). The other 7 bytes of the 16-byte
     struct stay zero from the prior zero-fill (r10 <= 15) or
     stay whatever was there (r10 > 15; only 256 bytes were
     zero-filled).

3. **ret at 0x54314**: post-ret IP = 0x54318.

The function builds a sprite/polygon descriptor table at
`0x01d03100` from src at `0x0201ea24` (12 B stride, 18 entries),
using either allocated (alloc path, r10 < 6) or fixed
(prealloc path, r10 >= 6) slot indices into the polygon pointer
table at `0x0201eafc` (32 entries of 4 B each).

## cmpdeco and cmpinco semantics

Two non-obvious branches required reading the repo's executor
implementation:

- **`cmpdeco 0, r15, r15`**: compare_unsigned(0, r15) sets CC
  (LESS if r15>0, EQUAL if r15==0); dst = r15 - 1.
  `bl` branches back if CC == LESS. The loop writes dst[0x1ff]
  down to dst[0], then with r15=0: CC=EQUAL, no branch, fall
  through. Net: 256 bytes zeroed.

- **`cmpinco 17, r10, r10`**: compare_unsigned(17, r10) sets CC
  (GREATER if r10<17, EQUAL if r10==17, LESS if r10>17);
  dst = r10 + 1 (clamped to 17 if r10>17).
  `bg` branches back if CC == GREATER. The loop runs while
  old_r10 < 17 (18 iterations: r10 = 0..17 at top of each iter).
  At r10=17, cmpinco sets r10=18, CC=EQUAL, bg no branch.

## Recovery shape

Standard 1-to-1 C translation in `src/recovered/hybrid.c`. The
alloc path calls `vf2_hybrid_timer_fuse_94d0_execute` (v0775),
which is a standalone recovery of `0x94d0` with a passed cpu
state. The cpu->ip is set to `0x94d0` before each call (matches
v0775's required entry IP), and g0 is read back from
`cpu->registers[G0_REGISTER]` after the call.

## Test

`tests/recovered/test_poly_record_load_5427c_native.c` (ctest #67).
4 legs:

- **Leg A**: zero src / zero poly_data -> verify zero-fill and
  body writes zero everywhere.
- **Leg B**: `(i+1)*0x01010101` src, `(i+0x12345670)*0x100` poly
  -> verify struct fill across both alloc (r10=0..5) and prealloc
  (r10=6..17) paths.
- **Leg C**: pre-fill dst with 0xAA sentinel -> verify zero-fill
  overwrites.
- **Leg D**: deadbeef src, distinct poly, assert post-call
  `0x500098` = 0x60 (state after 6 alloc calls with t0=1).

Timer setup: t0=1, t1=t2=t3=0. This makes `0x94d0` accumulate
`1<<4 = 0x10` per call, so consecutive calls return 1,2,3,4,5,6
(slots 1..6 for alloc). 0x500098 is reset to 0 before each leg.

Polygon table is filled with addresses (not values): each entry
`poly_tab[i] = POLY_DATA_BASE + i*4`. The deref in the recovery
then reads the corresponding poly data word from work_ram.

## Issues found

1. **`MAIN_DATA_SIZE`**: src at offset 0x1ea24 and poly_table at
   0x1eafc are within the first 128 KB of the main_data region,
   so the test buffer must be at least 0x20000 (128 KB), not the
   v0772 64 KB. With 64 KB, the first src write at
   `main_data[0x1ea24]` segfaults.

2. **Timer setup**: zero timers cause infinite alloc-loop
   (`0x94d0` always returns 0; bbs retry spins on bit 0 set in
   r6 after first alloc). t0=1 makes returns monotonically
   increase, breaking the loop.

3. **Polygon table semantics**: the values in `poly_table` are
   ADDRESSES, not data. The recovery dereferences them. Setting
   them to literal values (e.g. `i * 0x10000001`) causes the
   deref to read from garbage addresses and fail. Must point to
   the poly_data buffer.

## cascade closure

With v0776 done, all 4 sub-calls of `0x6de3c`'s record-rebuild
dispatcher are recovered:
- `0x60330` (v0774): record-clear
- `0x5427c` (v0776): record-load
- `0x94d0` (v0775): timer fuse (sub-call of v0776)
- `0x9480` (v0771): hash (eventually called from elsewhere)

The 4 sub-calls of `0x9427c` in the cascade (`0x6cf00`, `0x6cc00`,
`0x6d0a0`, `0x6de3c`) all call `0x60330`/`0x5427c`/`0x94d0`/`0x9480`
but those callers themselves remain un-recovered. Their sub-call
graphs are still deferred (64-bit carry arithmetic + multiplication
complexity per v0770 study).

## Files added/modified

- `src/recovered/hybrid.c` - v0776 recovery appended at end
- `include/vf2/hybrid/player.h` - `vf2_hybrid_poly_record_load_5427c_execute`
  declaration
- `decomp/i960/functions.csv` - row for 0x5427c at line 109
- `CMakeLists.txt` - ctest #67 entry
- `tests/recovered/test_poly_record_load_5427c_native.c` - 4-leg test

ctest: 0.09 s, 4 legs PASS.