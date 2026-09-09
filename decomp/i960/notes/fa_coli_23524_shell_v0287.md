# fa_coli `0x23524` shell native recovery — v0287

Native C recovery of the remaining interpreted shell inside `0x23524`.
The hybrid coli walk collapses to a single native procedure covering the
179-insn shell plus the `bal 0x23694` body, with every recovered callee
inlined for accounting only.

## What changed

- `coli_2396c_body` extracted from `vf2_hybrid_coli_2396c_execute`
  (export is now a thin wrapper). The standalone path is unchanged.
- Body-only helpers added for `0x233d0`, `0x238a4`, `0x238f8`, `0x2364c`.
- `vf2_hybrid_coli_23524_execute` implements the full warm shell.
- `hybrid_execute_coli_body` replaces the ~12 interpret/native windows
  inside `0x23524` with one native entry.

## Hybrid parent after v0287

```text
interpret 0x221e8 → 0x23524     # 7 insns
native    0x23524               # 9151
interpret 0x22210 → 0x22298
native    0x22298 ×2
interpret → 0x22404
native    0x22404 ×2
interpret → 0x10dcc
```

## Shell semantics (warm)

1. **Prologue** — `ldt +0x1f4` both fighters, float `subr` deltas,
   FIFO push `0x1f003e3e` + 5 words; clear 16 words at `g13+0x40`.
2. **Call glue** — FIFO `0x1d003a3a`; `g9 = 0x01000550`; `0x2396c` ×2.
3. **Flag builder** — `0x233d0` leaves `g6 = 0`; `bbc 0, g6` falls through.
4. **g3-scan** — `0x238a4` ×2 leave `g3 = 0`.
5. **Window / cluster** — FIFO `0x1d803b3b` + 5 words; 4 replies; store
   `g13+0x144/+0x148`; rolling average over `+0xf4/+0xf0/+0xec`, `g5 /= 4`.
6. **Nested scan** — `0x238f8` warm no-op (2855 insns).
7. **`bal 0x23694`** — reload fighters; `bbc 3, g6` clear; `cmpobl 0,
   g13+0x148` not taken; `0x2364c`; six-word store `g13+0xd4..0xe8`;
   float delta of `+0x644/+0x64c` against the zero cluster; FIFO
   `0x1e803d3d` + 6 words; fighter `+0x18/+0x20` update; `+0x650`
   threshold clamp with `0x3cf5c28f`.

## Fail-closed siblings

- `bbc 0, g6` at `0x235a0` taken (bit 0 set)
- `bbc 3, g6` at `0x236a4` taken (bit 3 set)
- `cmpobl 0, g13+0x148` taken (non-zero)
- every unmeasured child sibling (unchanged from v0284/v0285)

## Accounting

`hybrid_complete_procedure(body=9150, nested_calls=13, nested_returns=13)`
plus the completed ret yields **9151 / 13 / 14** for the procedure.
Caller `0x221e8` adds 7 insns + 1 call → path total **9158 / 14 / 14**.

## Validation

- unit test `test_coli_23524_shell` (insn/call/return pins, `g13+0x40`
  clear, flag-builder row, cluster/rolling/`0x2364c` stores, `g6`/`g4`)
- `ctest --test-dir build -C Debug` — **56/56 passed**
- PUNCH ROM-backed: **320/320 MATCH**, `12946` blocks,
  `14,962,620` instructions, both sides at `0x1645c`
- whole-task pin unchanged
