# fa_coli `0x23524` callee cost attribution — v0278

## Summary

Measurement-only slice. Attributes the 9,158-instruction warm `0x23524`
path to callees via a call-stack walk over the full guest trace. No C
recovery in this commit. Decides the first native leaf.

## Drive

From `out/coli-fail.vf2snap` (parked at `0x221e8`, flags `0x8a00`):

```text
vf2probe --snapshot out/coli-fail.vf2snap \
  --until 0x00022210 --max-steps 50000 --trace --memory-trace
```

- **9,158** instructions, **14** procedure calls / **14** returns
  (`42979/42974` absolute).
- final `ip = 0x22210`, status `ok`.
- 468 stores (462 u32 + 6 u16), 2,549 loads (v0275 totals).

## Per-callee attribution (innermost frame)

| function | entry | insns | invs | writes | role |
|----------|-------|------:|-----:|-------:|------|
| `fn_2396c` | `0x2396c` | **5236** | 2 | 402 | fighter poly cluster + FIFO |
| `fn_238f8` | `0x238f8` | **2855** | 1 | 0 | 30×30 nested bit-scan |
| `fn_23878` | `0x23878` | **810** | 6 | 0 | 30-iter bit-remap |
| `fn_23524` | `0x23524` | 179 | 1 | 53 | shell + window `0x884000` |
| `fn_233d0` | `0x233d0` | 44 | 1 | 6 | g6 flag builder |
| `fn_2364c` | `0x2364c` | 17 | 1 | 7 | FIFO delta push |
| `fn_238a4` | `0x238a4` | **10** | 2 | 0 | bit-8 early g3=0 |
| `coli_body` | `0x221e8` | 7 | 1 | 0 | entry prefix |
| unknown | — | 0 | — | — | — |

Total attributed: **9158**.

### Call sequence (warm, `bbc 0, g6` at `0x235a0` **not taken**)

```text
0x221e8 → call 0x23524
  0x23524 shell
    call 0x2396c   ret 0x23590   (inv 0; g7=f0)
    call 0x2396c   ret 0x2359c   (inv 1; g7=f1)
    call 0x233d0   ret 0x235a0
    bbc 0, g6 → 0x235ac   (not taken)
    call 0x238a4   ret 0x235b8   (inv 0)
    call 0x238a4   ret 0x235c8   (inv 1)
    ... polygon window / g13+0x144 cluster ...
    call 0x238f8   ret 0x23644
    bal  0x23694
      call 0x2364c ret 0x236b8   (at 0x236b4, after cmpobl 0,r3)
    ret 0x23648 → 0x22210
```

`0x2396c` itself calls `0x23878` three times per invocation
(`stos` into fighter `+0x624/+0x614/+0x618`).

## Measured per-invocation detail

### `0x238a4` — both invocations, early exit

Both take `g7+0x1a4` bit 8 **clear**:

```text
mov 0, r3
mov 0, g3
ld  0x1a4(g7), r15
bbc 8, r15, 0x238f4   # taken
ret
```

- **5 instructions** each, **0** counted calls, **1** return.
- Global `g3 = 0` survives `ret`; locals `r3`/`r15` are frame-restored.
- No memory writes.

### `0x23878` — bit remap, 6 invocations

Loop 30 times: if bit `r4` of `g3` is set, set bit `ROM[0x2007b76+r4*4]`
in the result; finally `g3 = result`. No memory writes.

| inv | insns | setbit | source g3 |
|----:|------:|-------:|-----------|
| 0 | 95 | 0 | 0 |
| 1 | 155 | 30 | all bits 0..29 |
| 2 | 155 | 30 | all bits 0..29 |
| 3 | 95 | 0 | 0 |
| 4 | 155 | 30 | all bits 0..29 |
| 5 | 155 | 30 | all bits 0..29 |

Empty path: `3 + 30*(bbc+cmpinco+bne) + 2 = 95`.
Full path: `3 + 30*(bbc+ld+setbit+cmpinco+bne) + 2 = 155`.
Table range `0x2007b76 .. 0x2007bea` (30 words).

### `0x238f8` — 30×30 nested scan, warm no-op

```text
for r6 in 0..29:
  r7  = ROM[0x23284 + r6]          # ldob
  r10 = mem[0x91f880 + r6*4]       # source mask
  for r8 in 0..29:
    if bit r8 of r10:
      r9  = ROM[0x23284 + r8]
      r11 = mem[g13+0x40 + r9*4]
      setbit r7, r11
      mem[g13+0x40 + r9*4] = r11
```

Warm path:

- **2855** instructions, **0** writes.
- Source table `0x91f880..0x91f8f4`: **30 reads of `0x00000000`**.
- Inner `setbit`/`st` branch never taken.
- Mnemonics: `cmpinco` 930, `bne` 930, `bbc` 900, `mov` 31, `ldob` 30,
  `ld` 30, `lda` 3, `ret` 1.
- Exact count: `4 + 30*(3 + 30*3 + 2) + 1 = 2855`.

### `0x23524` shell stores

- 20× `0x884000` (polygon window).
- 16-word clear of `g13+0x40` (`0x5149c0..`).
- Additional `g13+0x144/+0x148` cluster and `g11[g12]` FIFO slots.

### `0x2396c` stores (dominant)

- FIFO slots `0x514a8c` / `0x514a98` ×62 each.
- Fighter clusters via `stos g3` into `+0x624/+0x614/+0x618` (after
  `0x23878` remap).
- Writes total 402 of the 468 path stores.

## Implication for the next native child

**First leaf: `0x238a4`.** Smallest measured path (5 insns), same
bit-8-clear gate already proven on `0x22298`/`0x22404`, no memory
writes, only global `g3 = 0`. Establishes the hybrid parent of
`0x23524` so later leaves can be injected.

Next leverage after `0x238a4`:

1. `0x23878` — 810 insns, pure bit-remap, ROM table `0x2007b76`, no
   writes. Implement the 30-iter loop (or the measured empty/full
   extremes) and fail closed on unexpected shapes.
2. `0x238f8` — 2855 insns, warm no-op because source mask is zero.
   Recover the nested loop; the all-zero source is the warm early
   exit, not a license to always no-op.
3. `0x2396c` — 5236 insns, 30 blocks, writes fighter clusters. Larger
   semantic surface; recover only after the bit helpers are native.

Do **not** recover the `0x23524` 6-block shell alone.

## Hybrid segmentation target (v0279)

```text
interpret 0x221e8 → 0x23524
interpret 0x23524 → 0x238a4
native    0x238a4 → 0x235b8
interpret 0x235b8 → 0x238a4
native    0x238a4 → 0x235c8
interpret 0x235c8 → 0x22210
interpret 0x22210 → 0x22298
native    0x22298 → 0x22214
... (v0276/v0277 tail unchanged)
```

Whole-task pin remains **9214 / 18 / 19**.

## Proof

- Guest trace `9158` steps, attribution total `9158`, unknown `0`.
- `0x238a4` both invocations: 5 insns, `g3=0`, 0 writes.
- `0x238f8` source table all-zero on this drive.
- No snapshot/trace/ROM data is committed.
