# v0780 - 0x53750 display_profile_load recovered

## Scope

Recovered `0x53750` (108 B / 3 blocks / 22 instr) as a 1-to-1
C translation. Function reads a `src_base` pointer from
`main_data 0x0201fe68` into `g3`, sets `g4 = 0x00560000`
(work_ram), and copies `0xb01` 16-byte quads (`0xb010` =
45072 bytes) from `(g3)[r3*16]` to `(g4)[r3*16]` via a
`cmpinco/bg` loop with `r4 = 0xb00`. Then post-inits
`work_ram 0x5001a0..0x5001bf` (two 16B regions) with a
zero/`0x7f7f7f7f` pattern. No sub-calls.

## Disassembly

```
00053750  90983000  ld       0x0201fe68, g3
00053758  8ca03000  lda      0x00560000, g4
00053760  5c181e00  mov      0, r3
00053764  5922de08  shlo     8, 11, r4
00053768  b044de03  ldq      (g3)[r3*16], r8
0005376c  b2451e03  stq      r8, (g4)[r3*16]
00053770  5a18c204  cmpinco  r4, r3, r3
00053774  11fffff4  bg       0x00053768
00053778  5c781e00  mov      0, r15
0005377c  92783000  st       r15, 0x005001a0
00053784  92783000  st       r15, 0x005001b0
0005378c  8c703000  lda      0x7f7f7f7f, r14
00053794  92703000  st       r14, 0x005001a4
0005379c  92703000  st       r14, 0x005001b4
000537a4  5c701e00  mov      0, r14
000537a8  9a703000  stl      r14, 0x005001a8
000537b0  9a703000  stl      r14, 0x005001b8
000537b8  0a000000  ret
```

## cmpinco+bg semantics (re-derived)

`cmpinco r4, r3, r3` sets CC from `compare_unsigned(r4, r3)`
and:

- if `r3 <= r4`: `r3 = r3 + 1`
- else: `r3 = r4`

`bg` (branch if CC == GREATER, i.e. `r4 > r3`) branches back
to the loop head. The loop body is:

- `r3 < r4`: `r3 = r3 + 1`, CC = GREATER, bg taken (loop)
- `r3 == r4`: `r3 = r3 + 1`, CC = EQUAL, bg not taken (break)
- `r3 > r4`: `r3 = r4`, CC = LESS, bg not taken (break)

So with `r3 = 0, r4 = 0xb00`, the loop runs for
`r3 = 0..0xb00` (inclusive), i.e. `0xb01` iterations
copying `0xb010` bytes total.

**Initial v0780 commit had the loop off-by-one** (only
`r3 = 0..0xaff`, breaking when `r3 == r4`). The test fixture
caught it: status=7 (OUT_OF_BOUNDS) was actually reported
because the test pre-filled only 192 bytes and read past
the main_data buffer. Fix: loop `0..0xb00` inclusive, fill
test buffer with `0xb010` bytes, use `src_base = 0x02000000`
so reads stay within the 0x20000-byte attached buffer.

## Test

ctest #71: `vf2_display_profile_load_53750` (0.02 s) PASSES.
Four legs, each sets `src_base = 0x02000000`, fills
`0xb010` bytes with a different fill_byte, runs the
recovery, then verifies:

- 6 sample bytes in `dst[0x560000..0x56b00f]` match
  fill_byte (start, mid, end)
- 8 dword reads of `0x5001a0..0x5001bf` match the
  expected post-init pattern (0 / 0x7f7f7f7f / 0)

## File map

- `src/recovered/hybrid.c` (lines after v0779 commit):
  function `hybrid_execute_display_profile_load_53750` +
  wrapper `vf2_hybrid_display_profile_load_53750_execute`.
- `include/vf2/hybrid/player.h`: declaration.
- `decomp/i960/functions.csv`: row added for
  `0x53750..0x537bc display_profile_load`.
- `tests/recovered/test_display_profile_load_53750_native.c`:
  4-leg unit test.
- `CMakeLists.txt`: ctest #71 `vf2_display_profile_load_53750`.

## Caller context

Sole caller (per `vf2i960 xrefs 0x53750`): `0x98e4` in the
main-loop inline call table at `0x98a0..0x98ec`. This is a
display-profile load called at boot/post-boot. Not yet wired
into the dispatcher chain; the existing display-profile
recoveries (v0750 `0x1ff0c`, v0751 `0x1fffc`, v0754
`0x2eab8`) cover the 0x1fcc0 display sub-tree, and this
is a separate profile-load leaf in the main loop.
