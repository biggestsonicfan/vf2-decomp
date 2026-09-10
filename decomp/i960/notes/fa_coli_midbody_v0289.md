# fa_coli mid-body/tail warm path — v0289

## Summary

Closes the remaining interpreted glue in the `fa_coli` warm task.
After the v0287 native `0x23524` shell, the only interpreted piece was
the mid-body/tail from `0x22210` through the final ret to `0x10dcc`.
That 56-instruction corridor is now native. The warm `fa_coli` task
runs with **zero interpreted instructions** after the 7-insn entry
prefix.

## Measurement

From `out/coli-fail.vf2snap` (parked at `0x221e8`):

```text
vf2probe --snapshot out/coli-fail.vf2snap \
  --until 0x00022210 --max-steps 200000 \
  --output-snapshot out/coli-midbody-22210.vf2snap
# 9158 instructions from coli entry

vf2probe --snapshot out/coli-midbody-22210.vf2snap \
  --until 0x00010dcc --max-steps 500 --trace
# 56 instructions, 4 procedure calls, 5 procedure returns
```

### Exact warm path (`0x22210` → `0x10dcc`)

| addr | insn | effect |
|------|------|--------|
| `0x22210` | `call 0x22298` | first bitmask, original assignment |
| `0x22298` | body 6 + `ret` | bit 8 clear → `stos 0` at `g7+0x6dc` |
| `0x22214` | `mov r8,g7` | g7 = fighter1 (swap) |
| `0x22218` | `mov r7,g8` | g8 = fighter0 (swap) |
| `0x2221c` | `call 0x22298` | second bitmask, swapped |
| `0x22298` | body 6 + `ret` | same early exit |
| `0x22220` | `mov r7,g7` | g7 = fighter0 (restore) |
| `0x22224` | `mov r8,g8` | g8 = fighter1 (restore) |
| `0x22228` | `call 0x22404` | first contact, original |
| `0x22404` | body 13 + `ret` | snapshot store, clear slot bit, `g0 = 0` |
| `0x2222c` | `mov g0,r6` | r6 = first result (0) |
| `0x22230` | `mov r8,g7` | g7 = fighter1 (swap) |
| `0x22234` | `mov r7,g8` | g8 = fighter0 (swap) |
| `0x22238` | `call 0x22404` | second contact, swapped |
| `0x22404` | body 13 + `ret` | same early exit |
| `0x2223c` | `cmpobe 0,g0 → 0x22284` | taken (g0 = 0) |
| `0x22284` | `cmpobe 0,r6 → 0x22294` | taken (r6 = 0) |
| `0x22294` | `ret` | → `0x10dcc` |

Parent body excl. final ret: **13**. Children including their rets:
`7+7+14+14 = 42`. Total **55 + 1 completed ret = 56**. Nested
calls **4**, nested returns **4** + completed **1 = 5**.

Final `g7 = fighter1`, `g8 = fighter0` (swapped assignment from
`0x22230`/`0x22234`). `g0 = 0`, `g14 = 0x22428` (last bal link).

Parent locals `r7`/`r8` hold fighter0/fighter1 from the entry prefix
(`0x221f4`/`0x221fc`) but belong to the coli frame; the native mid-body
reloads the pointers from `0x500804`/`0x500808` instead.

## Recovery

`vf2_hybrid_coli_midbody_tail_execute`:

- preconditions: `ip == 0x22210`, `local_frame_depth > 0`
- reload fighter pointers from `0x500804`/`0x500808`
- inline `coli_22298_body` ×2 and `coli_22404_body` ×2 (body-only
  statics extracted from the v0276/v0277 exports; no CPU frame)
- fail-closed: bit 8 set on either fighter, slot > 1, or either
  contact-query result non-zero (path to `0x225cc` is unrecovered)
- leave `g7`/`g8` swapped, then `hybrid_complete_procedure(..., 55, 4, 4)`

`hybrid_execute_coli_body` now walks:

```
interpret 0x221e8 → 0x23524     # 7 insns entry prefix
native    0x23524               # 9151 insns
native    mid-body/tail         # 56 insns → 0x10dcc
```

## Proof

- Unit test `test_coli_midbody_tail_warm` pins `56 / 4 / 5`,
  both `+0x6dc` stores, swapped `g7`/`g8`, `g0 = 0`, `g14 = 0x22428`;
  bit-8-set sibling fails closed.
- `ctest -C Debug` **56/56 passed**
- ROM-backed PUNCH corridor:

  ```text
  vf2cycles --snapshot out/punch10.vf2snap --input 16 --cycles 320
  ```

  completed **320/320**, **12,946** compared blocks,
  **14,962,620** reference == native instructions,
  final `0x1645c` / `0x1645c`, **MATCH**.

- Whole-task coli pin remains **9214 / 18 / 19**.
- No snapshot/trace/ROM data is committed.

## Remaining `fa_coli` frontier

Warm path is fully native. Still open:

- unmeasured siblings of `0x22298` (bit 8 set, 16-trip loops)
- unmeasured siblings of `0x22404` (bit 8 set: polygon FIFO / hit path)
- `0x225cc` resolver (174 blocks) — needs a drive where a contact
  query returns non-zero
- bit-5-set early-ret path (already native as 3-insn gate)
