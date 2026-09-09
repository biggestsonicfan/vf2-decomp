# fa_coli contact query `0x22404` + warm-path skip of `0x225cc` — v0277

## Summary

Second mid-body native child inside the `fa_coli` warm body. Both
PUNCH-driven `call 0x22404` invocations take the same early exit.
The hybrid body now native-recovers `0x22298` ×2 and `0x22404` ×2.
The warm tail does **not** reach `0x225cc`: both contact-query results
are zero, so the caller jumps straight to `ret`.

## Warm-path measurement

From `out/coli-22298-r2.vf2snap` (parked at `0x22220`, after both
`0x22298` returns; fighters `g7=0x510800` / swapped `0x512800`;
registry `g13=0x514b80`):

```text
# First entry
vf2probe --snapshot out/coli-22298-r2.vf2snap \
  --until 0x00022404 --max-steps 200 \
  --output-snapshot out/coli-22404-e1.vf2snap
# 3 instructions: mov r7,g7 / mov r8,g8 / call

vf2probe --snapshot out/coli-22404-e1.vf2snap \
  --until 0x0002222c --max-steps 200 --trace --memory-trace
# 14 instructions, 0 counted calls, 1 return

# Second entry (g7/g8 swapped)
# +4 instructions: mov g0,r6 / swap / call
# another 14 instructions through ret to 0x2223c
```

Both invocations take the early exit (`g7+0x1a4` bit 8 **clear**):

| step | ip_before | mnemonic | effect |
|------|-----------|----------|--------|
| 1 | `0x22404` | `ldob 0x4(g7), r9` | slot = fighter index (0 then 1) |
| 2 | `0x22408` | `ldos 0x8c(g13)[r9*2], r3` | old snapshot (discarded) |
| 3 | `0x22410` | `ldos 0x1a8(g7), r4` | new snapshot value |
| 4 | `0x22414` | `stos r4, 0x8c(g13)[r9*2]` | **store** snapshot |
| 5 | `0x2241c` | `ld 0x1a4(g7), r15` | flags |
| 6 | `0x22420` | `bbs 8, r15, 0x2242c` | **not taken** (bit 8 clear) |
| 7 | `0x22424` | `bal 0x225bc` | link; not a counted call |
| 8–10 | `0x225bc` | `ldos/clrbit/stos 0x90(g13)` | **clear slot bit** |
| 11 | `0x225c8` | `bx (g14)` | return from bal; not a counted return |
| 12 | `0x22428` | `b 0x225b4` | |
| 13 | `0x225b4` | `mov 0, g0` | **g0 = 0** (global, survives ret) |
| 14 | `0x225b8` | `ret` | frame restore |

- instructions: **14** (13 body + 1 ret)
- procedure calls: **+0** (`bal`/`bx` are not counted; only `call`/`ret`)
- procedure returns: **+1**
- stores (fighter0 / fighter1 after swap):
  - `u16` fighter `+0x1a8` → registry `+0x8c` / `+0x8e`
  - clear bit slot in registry `+0x90` (both wrote `0` over `0`)
- measured values: slot `0` then `1`, snapshot source `0`, pending mask `0`

Local registers r3/r4/r9/r15 are discarded by the frame restore on `ret`.
`g0` is a global and must be explicitly set to `0`. `bal 0x225bc` links
`g14 = 0x22428`; `bx` does not clear it and `ret` does not restore
globals, so the recovery must also write `g14`.

## Caller tail (both results zero)

```text
0x2222c  mov  g0, r6          # first result (0)
0x22230  swap g7/g8
0x22238  call 0x22404         # second
0x2223c  cmpobe 0, g0, 0x22284   # taken
0x22284  cmpobe 0, r6, 0x22294   # taken
0x22294  ret                  # -> 0x10dcc
```

`0x22290 call 0x225cc` is **not reached** on the warm PUNCH path.
From `0x22220` through both contact queries to `0x10dcc` the tail is
38 instructions with the two `0x22404` calls.

## Recovery

`vf2_hybrid_coli_contact_query_execute`:

- preconditions: `ip == 0x22404`, `local_frame_depth > 0`
- read `g7+0x4` as slot; slot `> 1` → `VF2_ERROR_UNSUPPORTED`
- read `g7+0x1a4`; bit 8 **set** → `VF2_ERROR_UNSUPPORTED`
  (unmeasured sibling — the polygon-FIFO path at `0x224b4` and beyond)
- bit 8 **clear**:
  1. snapshot `*(u16*)(g7+0x1a8)` into `g13+0x8c+slot*2`
  2. clear bit `slot` in `*(u16*)(g13+0x90)`
  3. `g0 = 0`
  4. `g14 = 0x22428` (bal link)
  5. `hybrid_complete_procedure(..., 13, 0, 0)` (body 13 + ret 1 = 14)

## Hybrid segmentation

`hybrid_execute_coli_body` (v0277):

1. interpret `0x221e8` → stop `0x22298`
2. native `0x22298` → `0x22214`
3. interpret `0x22214` → stop `0x22298`
4. native `0x22298` → `0x22220`
5. interpret `0x22220` → stop `0x22404`
6. native `0x22404` → `0x2222c`
7. interpret `0x2222c` → stop `0x22404`
8. native `0x22404` → `0x2223c`
9. interpret `0x2223c` → `0x10dcc`

Whole-task pin remains the v0274 measurement: **9214 / 18 / 19**.
Native children advance counters by the same amounts the interpreter
would (7+7+14+14 instructions, +2+2 returns), so the pin is unchanged.

## Proof

- `ctest -C Debug` **56/56 passed**
- `vf2_native_runtime` unit test `test_coli_contact_query_22404_early_path`
  pins `14 / 0 / 1`, `g0 = 0`, `g14 = 0x22428`, snapshot `0x1234`,
  pending bit 0 cleared; bit-8-set sibling fails closed and leaves the
  poisoned snapshot/`+0x90` untouched
- ROM-backed PUNCH corridor:

  ```text
  vf2cycles --snapshot out/punch10.vf2snap --input 16 --cycles 320
  ```

  completed **320/320**, **12,946** compared blocks,
  **14,962,620** reference == native instructions,
  final `0x1645c` / `0x1645c`, **MATCH**.

- No snapshot/trace/ROM data is committed.

## Remaining `fa_coli` frontier

- `0x23524` subtree (interpreted; ~9,151 of the 9,214 warm instructions)
- `0x225cc` resolver (174 blocks) — **not on the warm PUNCH path**; needs
  a drive where either contact query returns non-zero (`g0`/`r6` such that
  the `cmpobe` at `0x2223c`/`0x22240` fall through to `0x22290`)
- unmeasured siblings of `0x22298` (bit 8 set, 16-trip loops)
- unmeasured siblings of `0x22404` (bit 8 set: polygon FIFO / hit path)
