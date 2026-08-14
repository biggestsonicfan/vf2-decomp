# Executable video table expand 128 (v0.0.26)

`0x00011704..0x00011740` is now fully reconstructed from the supplied VF2 2.1 ROM image.

```text
0x00011704  lda      0x12800000, g0
0x0001170c  lda      0x00078d10, g1
0x00011714  ld       0x00078d0c, g2
0x0001171c  shlo     7, 1, g3
0x00011720  ldob     (g1), r3
0x00011724  st       r3, (g0)
0x00011728  addo     1, g1, g1
0x0001172c  addo     4, g0, g0
0x00011730  cmpdeco  1, g3, g3
0x00011734  bl       0x00011720
0x00011738  cmpdeco  1, g2, g2
0x0001173c  bl       0x0001171c
0x00011740  ret
```

The outer count is data-driven from `0x00078d0c`; in the supplied VF2 2.1 ROM it is `0x42` (66). Each outer iteration expands 128 source bytes into 128 zero-extended 32-bit words, advancing the source by one byte and destination by four bytes per element.

Therefore the observed selector-3 call expands 8,448 bytes from `0x00078d10` into 33,792 bytes beginning at `0x12800000`.

The exact instruction formula is:

```text
3 setup instructions
+ N * (1 row setup + 128 * 6 inner-loop instructions + 2 outer-loop instructions)
+ 1 ret
= 4 + 771*N
```

For `N = 66`, the exact total is **50,890 instructions**.

For the normal nonzero-count path the final architectural state is:

```text
g0 = 0x12800000 + N * 0x200
g1 = 0x00078d10 + N * 0x80
g2 = 0
g3 = 0
r3 = final zero-extended source byte
compare = equal
```

The zero-count edge case is do/while-shaped in ROM: one row still executes, `g2` underflows to `0xffffffff`, and the final compare result is greater.

This function is now sufficiently bounded for native recovery. The remaining selector-3 parent blocker is `display_runtime_initialize` at `0x0002eab8`; once that child is executable, `display_profile_apply` at `0x0001fcc0` can be composed without retaining anonymous state injection for this corridor.
