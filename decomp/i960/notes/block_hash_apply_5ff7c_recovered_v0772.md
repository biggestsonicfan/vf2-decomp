# v0772 — `sub_00005ff7c` `block_hash_apply` recovered

Recovered the 40-byte, 1-block, 7-instruction thin wrapper
at `0x5ff7c..0x5ffa4` (one past the `ret` at `0x5ffa0`).
The wrapper reads a base pointer from work_ram, sets up
`g0`/`g1`/`g2`, calls the recovered CRC16 step hash at
`0x9480` (v0771), then stos the 32-bit result to
backup_sram.

## Disassembly (verified via `vf2i960 disasm ../../roms/vf2 0x5ff7c 16`)

```
0005ff7c  90783000  ld       0x0050016c, r15
0005ff84  8c83f400  lda      0x000033a8(r15), g0
0005ff8c  8c900644  lda      0x00000644, g2
0005ff90  5c881e00  mov      0, g1
0005ff94  09fa94ec  call     0x00009480
0005ff98  8a803000  stos     g0, 0x01d03304
0005ffa0  0a000000  ret
```

- `r15` = `*(uint32_t*)0x50016c` (work_ram pointer)
- `g0` = `r15 + 0x33a8` (1603-byte input buffer)
- `g1` = 0 (stride - 1, so stride = 1)
- `g2` = 0x644 (iteration count + 1, so 1603 iters)
- `call 0x9480` — the recovered CRC16 step hash
- `g0` after call = the 16-bit hash result (zero-extended via
  `mov r3, g0` inside 0x9480)
- `stos g0, 0x01d03304` — write 32 bits to backup_sram

## Recovery (`src/recovered/hybrid.c:36399-36464`)

`vf2_hybrid_block_hash_apply_5ff7c_execute` is a 1-to-1 C
translation. It dispatches the `call 0x9480` to the
already-recovered v0771 `vf2_hybrid_crc16_step_hash_9480_execute`
by:

1. Saving the current cpu->ip (0x5ff7c) implicitly via the
   caller's `enter_procedure` frame.
2. Setting `cpu->ip = 0x9480` (the 0x9480 recovery requires
   that exact IP for its entry check).
3. Calling the 0x9480 recovery.
4. Overwriting `cpu->ip` to the post-ret address `0x5ffa4`.

No refused sub-calls. The recovery returns `VF2_OK` or
propagates any error from the 0x9480 sub-recovery.

## Test (`tests/recovered/test_block_hash_apply_5ff7c_native.c`)

4 legs, each:

1. Fill a local `uint16_t table[256]` with the test pattern
   (default = `(~idx << 8) | idx`, linear = `idx*7 + 0x1234`).
2. Upload the table to the attached `main_data_buf`
   (main_data is read-only at the model2a boundary, so we
   pre-fill the attached buffer in C and let the recovery
   read it).
3. Write the input bytes to work_ram at `INPUT_BASE`
   (`0x00500800`, see "Layout" below).
4. Set `r15` at `0x50016c` to `INPUT_BASE - 0x33a8 = 0x4fd458`
   so the recovery's `lda 0x33a8(r15), g0` lands at
   `INPUT_BASE`.
5. Run the recovery.
6. Read the 32-bit result at `0x01d03304` (backup_sram) and
   compare against a 1-to-1 C implementation of the 0x9480
   hash on the same input + table.

| leg | input | table | g2 | recovered (stos) |
|---|---|---|---|---|
| A | identity (0..1602) | default | 0x644 | `0x0000d716` |
| B | 0xaa55 repeating | default | 0x644 | `0x000055aa` |
| C | stepping 0x10..0x00 | linear | 0x644 | `0x00001968` |
| D | all-0xff | linear | 0x644 | `0x000054c1` |

ctest entry #63, 0.04 s, PASS.

## What I learned while writing the test

### `INPUT_BASE` must NOT overlap `PTR_ADDR`

The first version of the test used `INPUT_BASE = 0x00500000`
and wrote the pointer to `0x50016c`. Then it wrote 1604
input bytes starting at `0x500000`, which **overwrote the
pointer** at `0x50016c` (offset `0x16c = 364` is well inside
the `0x644 = 1604`-byte input range). The recovery then
read garbage from `0x50016c` (specifically the bytes
`0x6c, 0x6d, 0x6e, 0x6f` of the identity input, which
little-endian form `0x6f6e6d6c` — hence the mystery value
seen during debugging).

The fix: move `INPUT_BASE` to `0x500800` (well past
`0x50016c + 0x644 = 0x5007b0`). Now the input write doesn't
trample the pointer.

This is a generic trap for tests that put both a pointer
and an input buffer in work_ram: the input range must
**not** overlap the pointer location. The current rule of
thumb in this test file: pick `INPUT_BASE` at offset
`>= 0x800` so it can hold up to `0x800` bytes of input
without touching the 4-byte pointer at offset `0x16c`.

### `0x50016c` is a "loaded" pointer, not data the test controls

Per the disasm, `0x50016c` is the *source* of `r15`, which
is then offset by `0x33a8` to find the input. So `r15` is
a runtime-loaded base, not a fixed address. The test must
set this pointer to whatever value makes
`r15 + 0x33a8 = INPUT_BASE`. For `INPUT_BASE = 0x500800`,
`r15 = 0x500800 - 0x33a8 = 0x4fd458`.

## `functions.csv` row

Added at line 23 (right after the v0771 row):

```
0x0005ff7c,0x0005ffa4,block_hash_apply,recovered-rom-anchor,rom-disassembly+unit,
40 B / 1 block / 7 instructions; thin wrapper around 0x9480
hash; reads base ptr from 0x50016c; sets g0=base+0x33a8
(1603-B input); g1=0 g2=0x644; calls 0x9480 then stos the
32-bit result to 0x01d03304 (backup_sram); recovered v0772;
extent 0x5ff7c..0x5ffa4 (one past the entry ret at 0x5ffa0);
called by gap-run siblings 0x6cf00 0x6cc00 0x6d0a0 per
study_deal_v0770,
```

`end` is `0x5ffa4` (one past `ret` at `0x5ffa0`); the
v0734a two-source rule is satisfied (vf2i960 function
reports the same extent, and `vf2i960 disasm` confirms
`0x5ffa0` is `0a000000 ret`).

## What is NOT yet done

- `0x5ff7c` is a standalone execute hook, not yet wired
  into the dispatcher chain. Per the v0755 / v0770 cascade
  map, wiring stays deferred.
- The next cascade step per `study_deal_v0770.md` is the
  gap-run siblings: `sub_00006cf00` (524 B) and
  `sub_00006cc00` (620 B). Both call `0x5ff7c` (now
  recovered) plus their own internal logic.
- After both siblings: `sub_00006d0a0` (108 B) at
  `0x6d0a0..0x6d108`, which calls `0x6d1b4` (recovered
  v0769) AND `0x5ff7c`.
- This slice is `src/` delta of one new function, one new
  ctest entry, and one new CSV row. The rest of
  `src/recovered/hybrid.c` and all other recovery slices
  are byte-identical except for the v0771 row that was
  also committed earlier in this session.
