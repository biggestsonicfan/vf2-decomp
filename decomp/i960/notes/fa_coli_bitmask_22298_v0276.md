# fa_coli mid-body child `0x22298` + hybrid segmentation — v0276

## Summary

First mid-body native child inside the `fa_coli` warm body. The
interpreter still covers the entry prefix, the `0x23524` subtree and the
contact/resolver tail; each `call 0x22298` is now recovered in C.

Establishes the hybrid segmentation pattern (interpret → native child →
interpret) that later children (`0x22404`, `0x225cc`) can reuse.

## Warm-path measurement

From `out/coli-fail.vf2snap` (parked at `0x221e8`, flags `0x8a00`):

```text
# First entry (after 0x23524 returns at 0x22210)
vf2probe --snapshot out/coli-fail.vf2snap \
  --until 0x00022298 --max-steps 20000 \
  --output-snapshot out/coli-22298-e1.vf2snap
# 9,159 instructions from coli entry

vf2probe --snapshot out/coli-22298-e1.vf2snap \
  --until 0x00022214 --max-steps 200 --trace --memory-trace
# 7 instructions, 1 return

# Second entry (g7/g8 swapped)
# +3 instructions: mov r8,g7 / mov r7,g8 / call
# another 7 instructions through ret to 0x22220
```

Both invocations take the same early exit:

| step | ip_before | ip_after | mnemonic | effect |
|------|-----------|----------|----------|--------|
| 1 | `0x22298` | `0x2229c` | `mov 0, r11` | r11 = 0 (discarded by ret) |
| 2 | `0x2229c` | `0x222a0` | `ld 0x1a4(g7), r7` | temp (discarded) |
| 3 | `0x222a0` | `0x222a4` | `ld 0x1a4(g8), r8` | temp (discarded) |
| 4 | `0x222a4` | `0x222a8` | `ldob 0x821(g8), r6` | temp (discarded) |
| 5 | `0x222a8` | `0x223b4` | `bbc 8, r8, 0x223b4` | **taken** (bit 8 clear) |
| 6 | `0x223b4` | `0x223b8` | `stos r11, 0x6dc(g7)` | **store 0** |
| 7 | `0x223b8` | ret site | `ret` | frame restore |

- instructions: **7**
- procedure calls: **+0**
- procedure returns: **+1**
- memory writes: `stos` u16 **0** at `g7+0x6dc`
- first write lands at `0x510edc` (fighter0 `0x510800`)
- second write lands at `0x512edc` (fighter1 `0x512800` after swap)

Local registers r6/r7/r8/r11 are written during the body and discarded
by the frame restore on `ret` (`executor.c` `procedure_return` memcpy
of the saved local-register set). Only the memory store survives.

## Recovery

`vf2_hybrid_coli_bitmask_execute`:

- preconditions: `ip == 0x22298`, `local_frame_depth > 0`
- read `g8+0x1a4`; bit 8 **set** → `VF2_ERROR_UNSUPPORTED`
  (unmeasured sibling — the 31-block loops at `0x222b0`/`0x22320`)
- bit 8 **clear** → `stos 0` to `g7+0x6dc`, then
  `hybrid_complete_procedure(..., 6, 0, 0)` (body 6 + ret 1 = 7)

## Hybrid segmentation

`hybrid_execute_coli_body` replaces the whole-body
`hybrid_execute_interpreted_task` for bit-5-clear:

1. interpret `0x221e8` → stop at `0x22298` (includes `0x23524`)
2. native `0x22298` → return to `0x22214`
3. interpret `0x22214` → stop at `0x22298` (swap + second call)
4. native `0x22298` → return to `0x22220`
5. interpret `0x22220` → `0x10dcc`

Whole-task pin remains the v0274 measurement: **9214 / 18 / 19**.
Native children advance counters by the same amounts the interpreter
would (7 + 7 instructions, +2 returns), so the pin is unchanged.

## Proof

- `ctest -C Debug` **56/56 passed**
- `vf2_native_runtime` unit test `test_coli_bitmask_22298_early_path`
  pins `7 / 0 / 1`, store 0, exit `0x22214`; bit-8-set sibling
  fails closed and leaves the pre-existing store untouched.
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
- `0x22404` contact query (24 blocks)
- `0x225cc` resolver (174 blocks)
- unmeasured siblings of `0x22298` (bit 8 set and the 16-trip loops)
