# v0774 — `sub_00060330` `poly_record_clear` recovered

Recovered the 220-byte, 1-block, 56-instruction tiny
record-clear function at `0x60330..0x60410` (one past
the `ret` at `0x6040c`). It clears a 15-byte record
in `backup_sram` (`0x01d03320..0x01d0332e`) and its
extended work_ram mirror (`0x59c320..0x59c32e`): the
first 9 bytes to `0` and the trailing 6 bytes to `2`.

## Disassembly (verified via `vf2i960 disasm ../../roms/vf2 0x60330 60`)

```
00060330  xor      r3, r3, r3            ; r3 = 0
00060334  st       r3, 0x01d03320        ; backup_sram 0x320..0x323 = 0
0006033c  st       r3, 0x0059c320        ; extended work_ram 0x320..0x323 = 0
00060344  stob     r3, 0x01d03324        ; 0
0006034c  stob     r3, 0x0059c324        ; mirror
00060354  stob     r3, 0x01d03325
0006035c  stob     r3, 0x0059c325
00060364  stob     r3, 0x01d03326
0006036c  stob     r3, 0x0059c326
00060374  stob     r3, 0x01d03327
0006037c  stob     r3, 0x0059c327
00060384  stob     r3, 0x01d03328
0006038c  stob     r3, 0x0059c328
00060394  mov      2, r15               ; init 0x329..0x32e to 2 (not 0)
00060398  stob     r15, 0x01d03329
000603a0  stob     r15, 0x0059c329
000603a8  mov      2, r15
000603ac  stob     r15, 0x01d0332a
000603b4  stob     r15, 0x0059c32a
000603bc  mov      2, r15
000603c0  stob     r15, 0x01d0332b
000603c8  stob     r15, 0x0059c32b
000603d0  mov      2, r15
000603d4  stob     r15, 0x01d0332c
000603dc  stob     r15, 0x0059c32c
000603e4  mov      2, r15
000603e8  stob     r15, 0x01d0332d
000603f0  stob     r15, 0x0059c32d
000603f8  mov      2, r15
000603fc  stob     r15, 0x01d0332e
00060404  stob     r15, 0x0059c32e
0006040c  ret
```

- 0 control flow (no branches, no calls)
- All writes are `st`/`stob` to fixed addresses
- First 9 bytes of the record → 0; last 6 bytes → 2
- Each write is mirrored to extended work_ram (the
  `0x59c000 + offset` pattern used everywhere in the
  gap-run code)

## Recovery (`src/recovered/hybrid.c` end)

`vf2_hybrid_poly_record_clear_60330_execute` is a 1-to-1
C translation. Two helper loops (one for the 0-bytes, one
for the 2-bytes) keep the recovery readable. No sub-calls.

## Test (`tests/recovered/test_poly_record_clear_60330_native.c`)

4 legs, each:

1. Pre-fill the full 15-byte range (and mirror) with a
   non-clear pattern.
2. Run the recovery.
3. Assert every byte in `0x01d03320..0x01d03328` (and
   mirror) is `0`; every byte in `0x01d03329..0x01d0332e`
   (and mirror) is `2`.

| leg | pre-state | result |
|---|---|---|
| A | all `0xff` | 0/2 in expected positions |
| B | all `0xaa` | 0/2 in expected positions |
| C | alternating `i & 0xff` | 0/2 in expected positions |
| D | all `0x00` (idempotent) | 0/2 in expected positions |

ctest entry, 0.04 s, PASS.

## Where this fits

- 0x60330 is the **sole** sub-call target of `0x6dea0`
  (per `vf2i960 xrefs 0x60330`), which sits inside
  `0x6de3c` — one of the 4 callers of the v0771 CRC16
  step hash at `0x9480` in the v0734k
  `0x4d2c0..0x4e808` gap run. So the call chain
  `0x6de3c` -> `0x60330` is now closed.
- The 4 callers of `0x9480` (0x6de3c, 0x6de6c, 0x6de90,
  0x6dec0) call `0x5427c` (156 B, 8 blocks, slot
  allocator + sprite/polygon loader) and `0x60330` (the
  record-clear, now recovered) before the hash call.
- The 15-byte record is adjacent to the 4-byte hash
  result at `0x01d03304` (written by v0772's `0x5ff7c`).

## `functions.csv` row

Added at line 25 (right after the v0773 row):

```
0x00060330,0x00060410,poly_record_clear,recovered-rom-anchor,rom-disassembly+unit,
"220 B / 1 block / 56 instructions; record-clear in
0x01d03320..0x01d0332e + 0x59c320..0x59c32e mirror
(15 bytes each; first 9 to 0; last 6 to 2); no
sub-calls; sole caller 0x6dea0 inside 0x6de3c (one of
the 0x9480 callers); recovered v0774; extent
0x60330..0x60410 (one past ret at 0x6040c)",
```

`end` is `0x60410` (one past `ret` at `0x6040c`); the
v0734a two-source rule is satisfied (vf2i960 function
reports the same extent, and `vf2i960 disasm` confirms
`0x6040c` is `0a000000 ret`).

## What is NOT yet done

- The 4 callers of `0x9480` (0x6de3c, 0x6de6c, 0x6de90,
  0x6dec0) are 912 B / 980 B / 980 B / 980 B — too large
  for one autonomous slice. They share the same shape:
  call `0x60330` (now done), call `0x5427c` (slot
  allocator), call `0x94d0` (timer fuse), then call
  `0x9480` and store the hash.
- `0x5427c` (156 B, 8 blocks) is the next candidate —
  it has a refused sub-call (`0x94d0`) and a 256-byte
  zero-fill loop, both tractable. The 0x5427c recovery
  would unblock all 4 callers.
- This slice is `src/` delta of one new function, one
  new ctest entry, one new CSV row. The rest of
  `src/recovered/hybrid.c` is byte-identical.
