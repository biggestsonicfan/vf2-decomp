# `display_profile_apply` control flow (v0.0.26)

Full ROM reconstruction of `0x0001fcc0..0x0001fee0` changes an important assumption made by the current selector-3 synthetic bridge.

## Complete bounded procedure

`0x0001fcc0` is a complete callable procedure ending at `0x0001fee0`. Its control flow is:

```text
0x0001fcc0  ld   0x00500804, r3
0x0001fcc8  ld   0x00500808, r4
0x0001fcd0  ldob 0x1b1(r3), r14
0x0001fcd4  cmpobne 2, r14, 0x0001fce0
0x0001fcd8  ldob 0x1b1(r4), r14
0x0001fcdc  cmpobe 1, r14, 0x0001fcf0
0x0001fce0  ldob 0x1b1(r3), r14
0x0001fce4  cmpobne 1, r14, 0x0001fd14
0x0001fce8  ldob 0x1b1(r4), r14
0x0001fcec  cmpobne 2, r14, 0x0001fd14
0x0001fcf0  lda  12, r15
0x0001fcf8  stib r15, 0x00500064
0x0001fd00  ld   0x00500068, r15
0x0001fd08  clrbit 20, r15, r15
0x0001fd0c  st   r15, 0x00500068
```

This first stage can select profile/mode `12` from the two runtime objects at `0x00500804` and `0x00500808`.

The next stage uses bits 20 and 21 of `0x00500068`, byte `0x00500064`, and byte `0x0050004c` to choose the profile constants:

```text
0x0001fd14  ld   0x00500068, r15
0x0001fd1c  bbc  21, r15, 0x0001fd2c
0x0001fd20  ld   0x00500068, r15
0x0001fd28  bbs  20, r15, 0x0001fd44
0x0001fd2c  ldob 0x00500064, r15
0x0001fd34  cmpobne 10, r15, 0x0001fd9c
0x0001fd38  ldob 0x0050004c, r14
0x0001fd40  cmpobe 2, r14, 0x0001fd8c
```

The mode-10 branch writes:

```text
0x00500064 = 10
set bit 20 of 0x00500068
0x0050a000 = 0x3a3117c4
0x0050a004 = 0x40000000
```

The alternate branch at `0x1fd8c` writes mode `11` and falls through to the default constants:

```text
0x00500064 = 11
clear bit 20 of 0x00500068
0x0050a000 = 0x3b32674f
0x0050a004 = 0x3f800000
```

The ordinary default path also clears bit 20 and writes the same `0x0050a000/004` pair without changing `0x00500064`.

## Table selection is not globally forced to profile 3

After calling `display_profile_mode_constants`, the parent routine reads `0x00500064` directly:

```text
0x0001fde4  ldob 0x00500064, r12
0x0001fdec  shlo 8, r12, r4
```

That `r4 = profile * 0x100` offset is then used for all of these table reads:

```text
0x0006eeae + r4 -> 0x00500170
0x0006eea4 + r4 -> 0x00501098
0x0006eea8 + r4 -> 0x00501020
0x0006eeaa + r4 -> 0x00501022
0x0006eeb0 + r4 -> g0 for video_command_submit
0x0006eeb4 + r4 -> g1 for video_command_submit
```

The code also uses table byte `0xae` to change `0x00501018` from `0x1388` to `0x10cc` when the value equals `4`.

Only after these reads does the parent call `0x0001fffc`.

## The force-to-3 behavior belongs only to `display_color_profile_apply`

`0x0001fffc` independently reloads the profile and runtime flags:

```text
0x0001fffc  ldob 0x00500064, r12
0x00020004  ld   0x00500068, r15
0x0002000c  bbc  21, r15, 0x00020018
0x00020010  lda  3, r12
0x00020018  shlo 8, r12, r4
```

Therefore bit 21 forcing profile `3` is local to the color-scale path. It selects only:

```text
0x0006eeb8 + r4
0x0006eeb9 + r4
0x0006eeba + r4
```

which are copied to `0x005000e0..0x005000e2` before `color_table_rebuild`.

This distinction matters because the current `selector3_apply_mode3_profile()` synthetic bridge uses one hard-coded table:

```c
table = 0x0006ee00 + 3 * 0x100;
```

for `0xae`, `0xa4`, `0xa8`, `0xaa`, and `0xb8..0xba` alike.

The ROM only proves the force-to-3 rule for `0xb8..0xba`. Using profile 3 for the parent routine's `0xa4/0xa8/0xaa/0xae/b0/b4` accesses requires dynamic evidence about the value of `0x00500064` at that exact observed visit. It must not be promoted to recovered semantics solely from the current synthetic post-state.

## Tail after table application

The rest of the parent is completely bounded:

```text
0x0001fe60  call 0x0001fffc
0x0001fe64  ld   0x0006eeb0(r4), g0
0x0001fe6c  ld   0x0006eeb4(r4), g1
0x0001fe74  call 0x0004b410

0x0001fe78..0x0001fed4
    clear 0x0050a014..0x0050a026

0x0001fed8  call 0x0002eab8
0x0001fedc  call 0x00011704
0x0001fee0  ret
```

All three call targets are now bounded and documented.

## Recovery rule

Do not rewrite the current selector-3 bridge as a literal translation of its hard-coded mode-3 poststate.

The executable recovery of `display_profile_apply` must preserve the real split:

```text
parent profile = 0x00500064 after parent control flow
color profile  = (flags bit 21) ? 3 : parent profile
```

Differential validation must capture `0x00500064`, `0x00500068`, the two fighter/object mode bytes, and `0x0050004c` at the relevant selector-3 visit. Only then can the synthetic delta be peeled without baking an accidental state snapshot into the recovered procedure.
