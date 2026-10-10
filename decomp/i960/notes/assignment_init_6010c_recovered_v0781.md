# v0781 - 0x6010c assignment_init recovered

## Scope

Recovered `0x6010c` (548 B / 1 block / 134 instr) as a 1-to-1
C translation. The function writes hardcoded default values
to the 29-byte assignment block at `backup_sram
0x01d03340..0x01d0335c` (and its `extended_work_ram` mirror
`0x0059c340..0x0059c35c`), then calls `0x5a460` (v0778
color_table_snapshot) to refresh the color table at
`work_ram 0x500234..0x50023a`.

Sole input: `g6` (held by the caller `0x6de3c`
record-rebuild dispatcher). Production value: `g6=6`,
yielding `r15 = g6 + 31 = 0x25 = 37` for the video-gain
bytes at `0x01d03359..0x01d0335b`.

## Disassembly

```
0006010c  mov  2,  r15 ; stob r15, 0x01d03340 ; stob r15, 0x0059c340
00060120  mov  2,  r15 ; stob r15, 0x01d03341 ; stob r15, 0x0059c341
00060134  mov  1,  r15 ; stob r15, 0x01d03342 ; stob r15, 0x0059c342
... (11 round-default 1's at 0x343..0x34e) ...
00060238  mov  15, r15 ; stob r15, 0x01d0334f ; stob r15, 0x0059c34f
0006024c  shlo 5,  5,  r15 ; stos r15, 0x01d03352 ; stos r15, 0x0059c352
00060260  shlo 3,  25, r15 ; stos r15, 0x01d03354 ; stos r15, 0x0059c354
00060274  mov  0,  r15 ; stob r15, 0x01d03351 ; stob r15, 0x0059c351
00060288  mov  0,  r15 ; stob r15, 0x01d03350 ; stob r15, 0x0059c350
0006029c  shlo 6,  1,  r15 ; stob r15, 0x01d03356 ; stob r15, 0x0059c356
000602b0  shlo 6,  1,  r15 ; stob r15, 0x01d03357 ; stob r15, 0x0059c357
000602c4  shlo 6,  1,  r15 ; stob r15, 0x01d03358 ; stob r15, 0x0059c358
000602d8  addo 31, g6, r15 ; stob r15, 0x01d03359 ; stob r15, 0x0059c359
000602ec  addo 31, g6, r15 ; stob r15, 0x01d0335a ; stob r15, 0x0059c35a
00060300  addo 31, g6, r15 ; stob r15, 0x01d0335b ; stob r15, 0x0059c35b
00060314  mov  31, r15 ; stob r15, 0x01d0335c ; stob r15, 0x0059c35c
00060328  call 0x5a460
0006032c  ret
```

## Stored values

| addr      | type   | value           | meaning                    |
|-----------|--------|-----------------|----------------------------|
| 0x340..1  | u8 x2  | 2               | MATCH COUNT (rounds)       |
| 0x342..e  | u8 xD  | 1 (x11)         | 11 per-round defaults      |
| 0x34f     | u8     | 15              | stage width                |
| 0x350..1  | u8 x2  | 0               | region header              |
| 0x352     | u16    | 0xa0 (160)      | ENERGY MAX 1               |
| 0x354     | u16    | 0xc8 (200)      | ENERGY MAX 2               |
| 0x356..8  | u8 x3  | 0x40 (64)       | video bias (1<<6)          |
| 0x359..b  | u8 x3  | (g6+31)         | video gain                 |
| 0x35c     | u8     | 31              | video scroll               |

All bytes mirrored to `0x0059c340..0x0059c35c`.

## Test

ctest #72: `vf2_assignment_init_6010c` (0.02 s) PASSES.
Four legs with `g6=6, 4, 0, 10` (gain bytes 0x25, 0x23,
0x1f, 0x29). Each leg:
- pre-fills `BS/EWR_BASE..+29` with `0xff` to detect
  any unwritten byte
- pre-fills `SNAP_BASE..+7` with `0xaa` and
  `BUS_PTR_VAL+SNAP_SRC_OFFSETS` with the 7 source
  bytes, so v0778's writes are verifiable
- runs the recovery
- asserts all 29 BS bytes match the documented defaults
- asserts all 29 EWR bytes match BS
- asserts all 7 SNAP bytes equal the pre-filled source
  bytes (proves v0778 ran)

## v0778 sub-call

`0x5a460` (v0778 color_table_snapshot) refuses its own
sub-call to `0x2b4c` (color_table_rebuild, not yet
recovered) and returns `VF2_ERROR_UNSUPPORTED` after
applying its 7 byte stores. v0781 tolerates that refused
return because the observable side effect (the 7 byte
snapshot) was already applied. **Sub-call IP must be set
explicitly**: `cpu->ip = 0x5a460` before invoking the
v0778 wrapper, matching the pattern used in v0779 for
`0x5ff7c`.

## File map

- `src/recovered/hybrid.c`: function
  `hybrid_execute_assignment_init_6010c` + wrapper
  `vf2_hybrid_assignment_init_6010c_execute`.
- `include/vf2/hybrid/player.h`: declaration.
- `decomp/i960/functions.csv`: row added for
  `0x6010c..0x60330 assignment_init`.
- `tests/recovered/test_assignment_init_6010c_native.c`:
  4-leg unit test.
- `CMakeLists.txt`: ctest #72 `vf2_assignment_init_6010c`.

## Caller context

Sole caller (per `vf2i960 xrefs 0x6010c`): `0x6de4c` in
the `0x6de3c` record-rebuild dispatcher. The dispatcher
calls `0x6010c` when the `0x01d03302` record CRC
(`0x9480` v0771 hash of the post-`0x3320` block) does
not match the stored value. After `0x6010c`, the
dispatcher calls `0x5427c` (v0776 poly_record_load) and
re-stores the new CRC.

This closes the last sub-call leaf in the
`0x6de3c` first block (other leaves in `0x6de3c`:
`0x9480` v0771, `0x5427c` v0776, `0x60330` v0774 are
already recovered; `0x6de3c` itself is deferred —
the dispatcher is too tangled with `0x2b4c` refusal
chains to safely recover 1-to-1).
