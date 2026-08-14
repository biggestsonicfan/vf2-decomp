# Selector 3 display initialization corridor (v0.0.26)

The remaining selector-3 synthetic bridge contains two more ROM-backed procedures that are now fully bounded from the reconstructed VF2 2.1 i960 image: `0x0002eab8` and `0x00011704`. Together they explain a large amount of state that had previously been represented only as post-state writes.

## `0x0002eab8` — `display_runtime_initialize`

This routine is called directly from `display_profile_apply` at `0x0001fed8`.

Its first block writes fixed display/runtime defaults:

```text
0x0002eab8  lda  0xc0900000, r15
0x0002eac0  st   r15, 0x0050a160
0x0002eac8  lda  0x3dcccccd, r15
0x0002ead0  st   r15, 0x0050a164
0x0002ead8  lda  0x3dcccccd, r15
0x0002eae0  st   r15, 0x0050a168
0x0002eae8  mov  0, r15
0x0002eaec  stib r15, 0x0050a14d
```

It then loads the runtime object pointer from `0x00500814` and initializes the block behind it. The observed writes include:

```text
base + 0x0df  clear bit 0
base + 0x234  0x3c872b02
base + 0x238  0x3ca3d70a
base + 0x23c  13
base + 0x23e  88
base + 0x23f  0
base + 0x240  0
base + 0x244  0
base + 0x246  0xff
base + 0x260  0
base + 0x264  0x409851ec
base + 0x268  0x40d051ec
base + 0x26c  0
base + 0x26e  0
base + 0x27c  0
base + 0x27d  0
base + 0x27e  0
base + 0x27f  0
base + 0x2b0..0x2c0  zeroed fields
base + 0x2c4  0x3e19999a
base + 0x2c8  0
base + 0x2cc  0xbcf5c28f
```

A second object pointer comes from `0x0050084c`; field `+0x40` is cleared. The routine then calls `0x00031004` and returns:

```text
0x0002ec0c  ld   0x0050084c, r4
0x0002ec14  mov  0, r15
0x0002ec18  st   r15, 0x40(r4)
0x0002ec1c  call 0x00031004
0x0002ec20  ret
```

This makes `0x0002eab8` a concrete display/runtime initializer rather than an opaque selector-3 side effect.

## `0x00031004` — `display_transform_defaults`

The nested helper is short and completely bounded:

```text
0x00031004  ld   0x0050084c, r4
0x0003100c  lda  0x40c00000, r8
0x00031014  lda  0x40966666, r9
0x0003101c  lda  0x41940000, r10
0x00031024  stt  r8, 0x54(r4)
0x00031028  mov  0, r12
0x0003102c  mov  r12, r13
0x00031030  mov  r12, r14
0x00031034  stt  r12, 0x60(r4)
0x00031038  st   r12, 0x70(r4)
0x0003103c  ret
```

The three constants written at `+0x54` are IEEE-754 bit patterns for approximately `6.0`, `4.7`, and `18.5`. The following triple and scalar state are cleared.

## `0x00011704` — `video_table_expand_128`

The caller at `0x0001fedc` invokes a second completely deterministic helper:

```text
0x00011704  lda  0x12800000, g0
0x0001170c  lda  0x00078d10, g1
0x00011714  ld   0x00078d0c, g2
0x0001171c  shlo 7, 1, g3
0x00011720  ldob (g1), r3
0x00011724  st   r3, (g0)
0x00011728  addo 1, g1, g1
0x0001172c  addo 4, g0, g0
0x00011730  cmpdeco 1, g3, g3
0x00011734  bl   0x00011720
0x00011738  cmpdeco 1, g2, g2
0x0001173c  bl   0x0001171c
0x00011740  ret
```

The ROM word at `0x00078d0c` is `0x42`, so the exact operation is:

```c
source = 0x00078d10;
destination = 0x12800000;
for (outer = 0; outer < 66; ++outer) {
    for (inner = 0; inner < 128; ++inner) {
        *(uint32_t *)destination = *(uint8_t *)source;
        ++source;
        destination += 4;
    }
}
```

Therefore it consumes exactly `66 * 128 = 8448` source bytes and produces `8448` zero-extended 32-bit destination entries (`33792` destination bytes by address span).

The source data begins with a monotonic repeated-byte ramp, so `video_table_expand_128` is best treated as a generic lookup-table expansion primitive until the consuming video hardware path gives stronger semantic evidence. It should not be named after a guessed gamma or color function yet.

## Updated selector-3 corridor

The ROM-backed profile/display path is now:

```text
0x0001fcc0  display_profile_apply
    |
    +--> 0x0001ff0c  display_profile_mode_constants
    |       +--> 0x0001fee4  display_profile_unit_fill
    |
    +--> 0x0001fffc  display_color_profile_apply
    |       +--> 0x00002c38  color_table_rebuild
    |
    +--> 0x0004b410  video_command_submit
    |
    +--> 0x0002eab8  display_runtime_initialize
    |       +--> 0x00031004  display_transform_defaults
    |
    +--> 0x00011704  video_table_expand_128
```

At this point every direct call in the observed tail of `0x0001fcc0` has a concrete ROM target and bounded behavior. The remaining recovery work is no longer target discovery: it is faithful composition and differential accounting.

## Recovery consequence

Do not reduce `VF2_SELECTOR3_INSTRUCTION_DELTA` yet. The current bridge still applies the final state atomically, and subtracting architectural counts before executing these recovered procedures would under-count the observed path.

The next implementation step is to encode these procedures as executable recovered helpers, compose them under the `0x0001fcc0` entry, compare all touched CPU registers and Model 2 memory against the existing selector-3 post-state, and only then peel their exact instruction/call/return counts away from the synthetic delta.
