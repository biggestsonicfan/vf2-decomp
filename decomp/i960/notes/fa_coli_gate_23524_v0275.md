# fa_coli bit-5-set gate + `0x23524` warm-call evidence — v0275

## Summary

Closes the last fail-closed hole in the `fa_coli` admission (bit-5-set
early return) as a measured native gate, and records the warm-path
boundary of callee `0x23524` for the next native child slice.

The bit-5-clear warm body remains the v0274 original-i960 bridge
(`9214 / 18 / 19` through `0x10dcc`).

## Bit-5-set gate (native)

From `out/coli-fail.vf2snap` (parked at `0x221e8`, flags `0x8a00`):

```text
vf2probe --snapshot out/coli-fail.vf2snap \
  --set-u32 0x00508000=0x8a20 \
  --until 0x00010dcc --trace --memory-trace
```

Measured path (`0x8a00 | 0x20`):

| step | ip_before | ip_after | mnemonic |
|------|-----------|----------|----------|
| 1 | `0x221e8` | `0x221f0` | `ld 0x508000, r15` |
| 2 | `0x221f0` | `0x22294` | `bbs 5, r15, 0x22294` (taken) |
| 3 | `0x22294` | `0x10dcc` | `ret` |

- instructions: **3**
- procedure calls: **+0**
- procedure returns: **+1**
- memory writes: **none**
- flags read: `0x00008a20`

Local `r15` is written during the body and discarded by the frame
restore on `ret`, so the native path does not materialize it.

### Recovery

`hybrid.c` `VF2_TASK_COLI_ENTRY`: when `0x508000` bit 5 is set, set
`body_instructions = 2` and complete the procedure via the shared
`hybrid_complete_procedure` path (`+2` body, `ret`, `+1` → total 3
instructions / 1 return). Bit 5 clear keeps the measured warm-body
bridge.

Unit test `test_coli_bit5_set_early_ret` pins the native poststate
(`3 / 0 / 1`, exit `0x10dcc`, flags unchanged) and keeps the
bit-5-clear synthetic case fail-closed.

## `0x23524` warm-call boundary (evidence only)

Same checkpoint, stop at the return address of the first call
(`0x22210`):

```text
vf2probe --snapshot out/coli-fail.vf2snap \
  --until 0x00022210 --max-steps 50000 --trace --memory-trace
```

Warm PUNCH path (bit 5 clear, flags `0x8a00`):

- **9,158 instructions** from `0x221e8` through the return of `0x23524`
  (entry prefix is 6 insns + `call`; the callee itself is ~9,151).
- **14 nested calls / 14 returns**.
- Call targets: `0x2396c`×2, `0x23878`×6, `0x233d0`×1, `0x238a4`×2,
  `0x238f8`×1, `0x2364c`×1.
  (`0x23878` is a 5-block bit-remap helper not listed in v0268.)
- `bbc 0, g6` at `0x235a0`: **not taken** → `0x235ac` path
  (`0x238a4` pair), not the `0x2364c` conditional block.
- 468 stores (462 u32 + 6 u16), 2,549 loads.
- Hot store addresses: `(g11)[g12]` FIFO slots `0x514a8c` / `0x514a98`
  (62 each) and polygon window `0x884000` (24).
- 16-word clear of `g13+0x40` lands at `0x5149c0`.
- Both fighters (`0x510800` / `0x512800`) receive the same offset
  cluster `+0xe80..+0xf04` — shared-layout evidence, still provisional
  (`field_xxx`).

Dominant mnemonics: `bbc`, `ld`, `cmpinco`/`bne` loops, `addr`,
`mulr` — the 9k count is loop-driven inside `0x2396c`/`0x233d0`, not
the 6-block prefix itself.

### Implication for the next native child

`0x23524` is **not** a small prefix: it accounts for ~99% of the warm
body instruction count. A native recovery of just the 6-block shell is
useless without the callee subtree. Next leverage order:

1. segment hybrid (interpret entry → native child → interpret tail)
   or recover `0x22298` (31 blocks, no calls) as the first mid-body
   native child;
2. then attack `0x23524` as a measured subtree (or its hot callees
   `0x2396c` / `0x233d0`) with the boundary package above.

## Proof

- `ctest -C Debug` focused suites (`vf2_tests`,
  `vf2_native_runtime`, `vf2_native_runtime_state`,
  `vf2_native_differential`, `vf2_object_handlers_differential`) pass.
- Bit-5-clear PUNCH corridor path is unchanged (v0274 bridge).
- No snapshot/trace/ROM data is committed.
