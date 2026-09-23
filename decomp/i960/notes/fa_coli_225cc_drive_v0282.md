# fa_coli `0x225cc` reachability drive + scanbit/bno fix — v0282

## Summary

Mutation-only slice plus one executor semantic fix. Establishes a
measured drive that returns `g0 != 0` from the contact query at
`0x22404` and reaches the previously unreachable resolver `0x225cc`
(174 blocks). Documents the `scanbit`/`bno` NoBit interaction that
blocked the drive.

## Executor fix: `bno` after `scanbit`

`vf2_i960_step_legacy` implemented `bno` as `compare != OVERFLOW`
(correct for arithmetic overflow) and `scanbit` set `EQUAL` on a hit.
Because `EQUAL != OVERFLOW`, **`bno` was taken after a successful
`scanbit`**, so every scanbit-loop exited on the first iteration.

Measured on the `0x22404` bit-8-set sibling (`out/coli-22404-e1`):

```text
g7+0x1a4 bit 8 set, g7+0x820 = 1
  -> table[1] at 0x02007aca = 8 (bit 3)
  -> scanbit r3, r5 with r3 = 8
  -> bno was still taken (pre-fix), mask stayed empty, g0 = 0
```

After the fix:

- hit: `dest = bit index`, `compare = OVERFLOW` (bno not taken)
- miss: `dest = 31`, `compare = NONE` (bno taken)

Unit test in `tests/i960/test_executor.c`. Warm PUNCH corridor is
unchanged (`320/320` MATCH); the warm path never executes `scanbit`
with a non-zero source.

## Drive to `g0 = 1` (first contact query)

From `out/coli-22404-e1.vf2snap` (parked at the first `0x22404`,
`g7 = 0x510980`, `g13 = 0x514b80`):

```text
vf2probe --snapshot out/coli-22404-e1.vf2snap \
  --set-u32 0x00510b24=0x100 \
  --set-u8  0x005111a0=0x01 \
  --set-u32 0x005149cc=0xffff \
  --until 0x0002222c --max-steps 800 --trace
```

| mutation | field | effect |
|----------|-------|--------|
| `g7+0x1a4 = 0x100` | flags | bit 8 set → polygon path |
| `g7+0x820 = 1` | scan index | `0x02007aca[1] = 8` |
| `0x5149cc = 0xffff` | dest slot | non-empty mask after `or` |

Warm values that already satisfied the other gates: pending bit clear
(`g13+0x90 = 0`), threshold `g7+0x1aa >= g7+0x808` (`0 >= 0`),
`0x223bc` returns 0, `g8+0x6dc = 0`.

First query: **73 instructions**, `g0 = 1`, ret to `0x2222c` via
`0x225b0` (the `mov 1, g0` path).

## Drive to `0x225cc`

Same mutations, stop at `0x225cc`:

```text
--until 0x000225cc --max-steps 2000
```

- **96 instructions** from `0x22404-e1`
- final `ip = 0x000225cc`
- `procedure_calls` +2 (second `0x22404` and `0x225cc`)
- `procedure_returns` +2

Caller tail after the first query returns 1:

```text
0x2222c  mov  g0, r6          # r6 = 1
0x22230  swap g7/g8
0x22238  call 0x22404         # second query, g0 = 0 (warm sibling)
0x2223c  cmpobe 0, g0, 0x22284   # taken (second result 0)
0x22284  cmpobe 0, r6, 0x22294   # not taken (r6 = 1)
0x22290  call 0x225cc
```

Only the **first** contact query needs `g0 != 0`.

Entry snapshot (local scratch): `out/coli-225cc-entry.vf2snap`.

## `0x225cc` prefix (not recovered)

```text
ld   0x1234(g7), r15
lda  1(r15), r15
st   r15, 0x1234(g7)          # counter++
ldob 0x19f(g8), r14
cmpobne 22, r14, 0x225f0
call 0x18bd4                  # known native (player/geometry)
b    0x230b8
... scanbit / FIFO / mulr path ...
```

Warm drive does **not** take the `call 0x18bd4` shortcut unless
`g8+0x19f == 22`. The rest of the 174-block body is unmeasured on this
drive.

## Proof

- `ctest -C Debug` **56/56 passed**
- ROM-backed PUNCH corridor **320/320 MATCH** (`14,962,620` instructions)
- scanbit unit test pins hit (`dest=3`, OVERFLOW) and miss (`dest=31`,
  NONE)
- No snapshot/trace/ROM data is committed
