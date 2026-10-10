# v0771 — `sub_00009480` `poly_or_crc16_step_hash` recovered

Recovered the 80-byte, 3-block, 16-instruction `cmpdeci`-driven
table-lookup hash at `0x9480..0x94d0` (one past the `ret` at
`0x94cc`). The function reads a 16-bit accumulator through
`main_data[0x02000000 + idx*2]`, folding in each input byte via
a rotate-and-XOR-and-mask pattern, then returns the
accumulator through `g0`.

## Disassembly (verified via `vf2i960 disasm ../../roms/vf2 0x9480 32`)

```
00009480  5c181e00  mov      0, r3
00009484  8c503000  lda      0x0000ff00, r10
0000948c  8c5800ff  lda      0x000000ff, r11
00009490  594c8901  subo     1, g2, r9
00009494  5928ce08  shlo     8, r3, r5
00009498  5829408a  and      r10, r5, r5
0000949c  5918cc08  shro     8, r3, r3
000094a0  80341000  ldob     (g0), r6
000094a4  59840801  addo     1, g0, g0
000094a8  59840011  addo     g1, g0, g0
000094ac  5818c306  xor      r6, r3, r3
000094b0  5818c08b  and      r11, r3, r3
000094b4  88183883  ldos     0x02000000[r3*2], r3
000094bc  5818c305  xor      r5, r3, r3
000094c0  5a4a4b80  cmpdeci  0, r9, r9
000094c4  14ffffd0  bl       0x00009494
000094c8  5c801603  mov      r3, g0
000094cc  0a000000  ret
```

- `g0` = input pointer (advances by `1 + g1` per iteration)
- `g1` = stride - 1
- `g2` = iteration count + 1 (so `r9 = g2 - 1` initial)
- Loop: `cmpdeci 0, r9, r9` decrements `r9`, then `bl 0x9494`
  branches if `signed(r9) > 0`. Net: runs exactly `g2 - 1` times.
- Each iteration: r3 = (r3 >> 8); r3 ^= input_byte; r3 &= 0xff;
  r3 = main_data[r3 * 2]; r3 ^= ((r3_prev << 8) & 0xff00).

## Recovery (`src/recovered/hybrid.c:36271-36369`)

`vf2_hybrid_crc16_step_hash_9480_execute` is a 1-to-1 C
translation of the disassembly. The `cmpdeci` semantics are
emulated as "decrement first, then continue if signed(r9) > 0"
— the same `compare_signed(cpu, 0, r9)` and
`condition_matches("bl", result)` behaviour the reference
executor uses (`src/i960/executor.c:249, 324`).

The recovery refuses any sub-call (none to refuse — pure
in-line) and writes the 16-bit result to `g0` and sets
`cpu->ip = 0x94d0` (one past the `ret`).

## Test (`tests/recovered/test_crc16_step_hash_9480_native.c`)

4 legs, each:

1. Fill a local `uint16_t table[256]` with the test pattern
   (default = `(~idx << 8) | idx`, linear = `idx*7 + 0x1234`).
2. Upload the table to a `static uint8_t main_data_buf[0x10000]`
   via direct C writes (main_data is read-only by hardware
   design, so `vf2_model2a_write` to `0x02000000` is refused
   by the model). The buffer is attached to the model2a via
   `vf2_model2a_attach_main_data` so the recovery sees the
   same backing.
3. Write the input bytes to `work_ram` at `INPUT_BASE`
   (`0x00580400`, inside the auto-allocated work_ram).
4. Set `g0`/`g1`/`g2` and call the recovery.
5. Read the table back via `vf2_model2a_read` into a local
   buffer and run a naive 1-to-1 C implementation.
6. Assert `naive == recovered`.

| leg | input | table | g2 (=n+1) | recovered |
|---|---|---|---|---|
| A | `{0x01, 0x02, 0x03, 0x04}` | default | 5 | `0xf806` |
| B | `0xaa55` × 8 (4 pairs) | default | 9 | `0x5500` |
| C | `0x10..0x00` stepping (16 B) | linear | 17 | `0x80d5` |
| D | `{0xff}` (1 B, g2=1) | linear | 2 | `0x192d` |

ctest entry #62, 0.04 s, PASS.

## What I learned while writing the test

### `main_data` is read-only at the model2a boundary

`find_view` in `src/hardware/model2a.c:162-167` sets
`view->write_data = NULL` for the main_data region. The
`vf2_model2a_write` check at line 858 returns
`VF2_ERROR_OUT_OF_BOUNDS` for any write to `0x02000000`.

This is a hardware boundary: main_data is read-only at the
model2a level. To "set up" a table, the test must:
1. Allocate its own buffer.
2. Fill it in C (no model2a involved).
3. Attach it via `vf2_model2a_attach_main_data` so the
   recovery's reads find the backing.

### `cmpdeci 0, r9, r9; bl` semantics

The first version of the recovery checked the loop-continuation
condition BEFORE the decrement. That gave N+1 iterations
instead of N. Correct order: decrement first, then test
`signed(r9) > 0` (matches `condition_matches("bl", ...)` in
`src/i960/executor.c:324` after `compare_signed(cpu, 0, r9)`
sets `LESS` for `0 < r9`).

This is the same kind of off-by-one trap that bit v0751's
`0x1fffc` test (wrong mask polarity) — read the reference
executor's condition-matching table before guessing the
semantics.

## `functions.csv` row

Added at line 22 (right after the `0x00009444`
`inline_text_thunk` continuation row):

```
0x00009480,0x000094d0,poly_or_crc16_step_hash,recovered-rom-anchor,rom-disassembly+unit,
"80 B / 3 blocks / 16 instructions; 16-bit accumulator hash
 with 256-entry 16-bit table at 0x02000000 (VF2_MAIN_DATA_BASE);
 cmpdeci-then-bl loop 'cmpdeci 0 r9 r9; bl 0x9494'; 4 callers
 all in 0x4d2c0..0x4e808 gap run; recovered v0771; extent
 0x9480..0x94d0 (one past the entry's ret at 0x94cc)",
```

`end` is `0x94d0` (one past `ret` at `0x94cc`); the v0734a
two-source rule is satisfied (vf2i960 function reports the
same extent, and `vf2i960 disasm` confirms `0x94cc` is
`0a000000 ret`).

The notes are QUOTED because the assembly snippet
`cmpdeci 0 r9 r9` would otherwise parse as a field separator
(v0734b guard: `test_real_csv_notes_survive_the_commas_they_contain`).

### Side effect: re-quoted 33 unquoted-comma rows

While writing the v0771 row I tripped the v0734b guard
`test_real_csv_notes_survive_the_commas_they_contain` in
`vf2_python_factory_block_coverage` (ctest #149). The guard
asserts every row in `functions.csv` parses to exactly 7
fields. My v0771 row had an i960-assembly fragment
(`cmpdeci 0,r9,r9`) whose commas would silently split the
notes into 3+ fields if unquoted — the same failure mode
v0734b added the guard to catch.

While fixing it I found 33 pre-existing rows with the same
issue, including the v0765
`poly_or_model2a_register_switch` row I had added in this
session. All 33 had unquoted commas in their notes. The
quotes are essential: per v0734b, a `DictReader` consumer
that doesn't specify `restval` silently drops the
post-comma text into `None`. The v0734b ctest pins the
fix; re-quoting 32 v0734b-era rows restores the field count
to 7, and the v0765 row gets a fresh quote (it had been
shipped unquoted — a v0765-era bug).

## What is NOT yet done

- The 4 callers (`0x6de3c`, `0x6de6c`, `0x6de90`, `0x6dec0`)
  are all in the v0734k gap run `0x4d2c0..0x4e808`. They are
  not yet recovered themselves.
- `sub_00009480` is a standalone execute hook, not yet wired
  into the dispatcher chain. Per the v0755 / v0770 cascade
  map, wiring stays deferred until the surrounding
  `0x4d2c0..0x4e808` gap run is recovered.
- The next cascade step per `study_deal_v0770.md` is
  `sub_00005ff7c` (40 B), then `sub_00006cf00` / `0x6cc00`
  (524 / 620 B writers that call `0x5ff7c` and `0x9480`).
- This slice is `src/` delta of one new function and one new
  ctest entry only; the rest of `src/recovered/hybrid.c` and
  all other recovery slices are byte-identical.
