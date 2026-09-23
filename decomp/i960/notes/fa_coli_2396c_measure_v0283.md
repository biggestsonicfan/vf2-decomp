# fa_coli remaining-leaf measure — v0283

Measurement-only slice. Reconfirms the warm PUNCH corridor and attributes
every remaining interpreted instruction inside `0x23524` to a concrete
block path. No C recovery in this commit.

## Drive

Rebuild Debug (`VF2_BUILD_TESTS=ON`, `VF2_WARNINGS_AS_ERRORS=ON`,
`VF2_ROM_DIR=roms/vf2`).

PUNCH corridor from `out/punch10.vf2snap` (`--input 16 --cycles 320`):

```text
Requested/completed cycles:         320/320
Compared blocks:                    12946
Reference instructions:             14962620
Recovered native instructions:      14962620
Final reference/native address:     0x0001645c/0x0001645c
Endurance result:                    MATCH
```

Warm path from `out/coli-fail.vf2snap` (parked at `0x221e8`):

```text
vf2probe --snapshot out/coli-fail.vf2snap \
  --until 0x00022210 --max-steps 50000 --trace --memory-trace
```

- **9,158** instructions, final `ip = 0x22210`, status `ok`.
- 3,017 memory events (reads+writes) in this capture.
- Trace saved as `out/coli-23524-v0283.jsonl` (local only; not committed).

## Innermost-frame cost (reconfirmed)

| function | insns | invs | writes | role |
|----------|------:|-----:|-------:|------|
| `0x2396c` | **5236** | 2 | 402 | fighter poly cluster + FIFO |
| `0x238f8` | **2855** | 1 | 0 | 30×30 nested bit-scan (native v0281) |
| `0x23878` | **810** | 6 | 0 | 30-iter bit-remap (native v0280) |
| shell `0x23524` | 179 | 1 | 53 | window `0x884000`, clear `g13+0x40` |
| `0x233d0` | 44 | 1 | 6 | g6 flag builder + ROM table copy |
| `0x2364c` | 17 | 1 | 7 | FIFO delta push |
| `0x238a4` | 10 | 2 | 0 | bit-8 early g3=0 (native v0279) |
| coli entry | 7 | 1 | 0 | `0x221e8` prefix |
| **total** | **9158** | | | |

Already native: 3675. Remaining interpreted: **5476**.

## `0x2396c` — go (large, split candidate)

`function 0x2396c`: address `0x0002396c`, end `0x00023bb8`, **30 blocks**,
no indirect calls.

### Warm block map

Every block that the warm path can reach is reached; the untaken sides are
a small, fixed set. Per-invocation own cost (excluding nested `0x23878`
bodies, including the three `call` insns and the final `ret`): **2618**.
Total including three `0x23878` bodies: **3023** per invocation.

Untaken warm blocks (fail-closed siblings):

| block | meaning |
|-------|---------|
| `0x23a70` | setbit `g13+0x110` — never fires |
| `0x23a88` | setbit `g13+0x114` — never fires |
| `0x23af0` | inner positive-threshold path — never taken |
| `0x23b14` | inner min-compare — never taken |
| `0x23b24` | inner min-store — never taken |
| `0x23b88` | max-count update — never taken |

Always-taken warm conditionals (30 outer × 2 fighters = 60):

- `0x23a54` `bg` not taken → setbit `g13+0x10c` **always**
- `0x23a6c` `bg` taken → skip `g13+0x110`
- `0x23a84` `bg` taken → skip `g13+0x114`
- `0x23a98` `bbs 2` not taken
- `0x23aa0` `bbs 23` not taken
- `0x23aac` `bg` not taken → setbit `g13+0x118` **always**

Inner loop (4 trips, 240 hits):

- `0x23aec` `cmpibg` taken → skip positive path
- `0x23b10` `bbc` taken → skip min path

Final max loop (4 trips):

- `0x23b84` `cmpibge` taken → max stays 0

### Nested calls

Exactly three `call 0x23878` per invocation, from `0x23b40`, `0x23b54`,
`0x23b60` (returns `0x23b44`, `0x23b58`, `0x23b64`).

### Stores (402 events, 201 unique addresses)

Fighter cluster `stq` into `g7+0xd00[i]` for `i=0..29` (4 u32 each):

- inv0: `0x00511680` .. `0x0051185c` (120 u32)
- inv1: `0x00513680` .. `0x0051385c` (120 u32)

`stos` after remap (size 2 each):

- `g7+0x624`, `g7+0x614`, `g7+0x618`

Final pair:

- `g7+0x644`, `g7+0x64c` (u32)

FIFO-side words at `0x00514a8c` / `0x00514a98` (62 each) plus neighbors
`0x00514a7c..0x00514ab8` (count 2 each). These are the `g13` threshold /
accumulator cluster (`+0xfc..+0x12c`) written every invocation.

### Entry package (inv0 / inv1)

| item | inv0 | inv1 |
|------|------|------|
| `g7` | `0x00510980` | `0x00512980` |
| `g7+4` (slot) | `0x00` | `0x01` |
| poly base via `0x23944[slot]` | `0x00fa9000` | `0x00fa9100` |
| first source `ldt` | `0x0090fa00` | `0x0091fa00` |
| ROM `0x2394c[0]` | `0x01` | `0x01` |
| ROM `0x232c4` sample | `b6f3fd3d` at `0x020078c4` | same |

### Verdict

**GO**, but it is the largest semantic surface currently open. Prefer a
two-step slice: (a) extra measure + C prototype not wired into the hybrid
parent; (b) wire-up replacing the six-step `0x23878` walk with two native
`0x2396c` entries. The bit-remap body must be inlined (or factored without
`hybrid_complete_procedure`) because calling `vf2_hybrid_coli_23878_execute`
from inside a native `0x2396c` would pop the wrong frame.

Insn accounting for one native invocation:

```text
body     = 2617          # 2618 minus the final ret (complete adds it)
nested_calls    = 3
nested_returns  = 3      # one per inlined remap ret
+ three remap bodies of (94 + 2*set_bits) each
+ one ret of 0x2396c
```

Warm `set_bits` sum per invocation is 60 (405 = 3×95 + 2×60 total nested).

## `0x233d0` — go

44 instructions, **one** measured path, 6 writes, 0 nested calls, 1 return.

Entry `0x233d0` is a late entry that reloads `g7`/`g8` from `0x500804` /
`0x500808`, builds `or`/`and`/`xor` of both fighters' `+0x1a4`, clears
`g6`, then branches **backwards** into the shared body at `0x23398`.

Warm taken path (all siblings fail-closed):

```text
0x233d0  g7/g8 from globals; r7/r8 = +0x1a4 (both 0); g6 = 0
0x23398  ldos +0x1a8 both (both 0); none of {0x242, 0x241, shlo 2,27}
         match  -> b 0x233fc
0x233fc  bbc 18, r9        taken (or bit 18 clear)
0x23408  bbc 8,  r11       taken (xor bit 8 clear)
0x23428  bbc 8,  r10       taken (and bit 8 clear)
0x23434  bbc 14, r9        taken (or bit 14 clear)
0x23440  r12 = 0x232c4
0x23448  bbs 0, g6         not taken
0x2344c  bbs 2, g6         not taken
0x23450  bbc 16, r7        taken
0x2347c  bbc 16, r8        taken
0x234a8  bbc 1, g6         taken
0x234f0  copy 6 words from ROM 0x232c4 into g13:
           +0xb4, +0xb8, +0xc0, +0xc4, +0xbc, +0x88
0x23520  ret -> 0x235a0
```

Measured ROM row (`0x232c4`): `0x00000000, 0x00000000, 0x0000003f,
0x0000003f, 0x0002336c, 0x00000000`.

Measured stores (`g13 = 0x00514980`):

| dest | value |
|------|------:|
| `g13+0xb4` | `0` |
| `g13+0xb8` | `0` |
| `g13+0xc0` | `0x3f` |
| `g13+0xc4` | `0x3f` |
| `g13+0xbc` | `0x0002336c` |
| `g13+0x88` | `0` |

`shlo 2, 27, r15` is `27 << 2 = 108` per the executor (`second << first`).
Both `ldos +0x1a8` values are 0 on warm, so no magic compare fires.

`g6` leaves as 0. Locals (`r3`/`r4`/`r7`..`r15`) are frame-restored.

### Verdict

**GO.** Compact single-path leaf. Fail-closed on every unmeasured sibling
including the six magic `+0x1a8` compares and every `bbc`/`bbs` that the
warm path does not take.

## `0x2364c` — go

17 instructions, **one** measured path, 7 writes (4 FIFO + 3 `g13`), 0
nested calls, 1 return. Caller `0x23694` reloads `g7`/`g8` from
`0x500804`/`0x500808` and only calls after `cmpobl 0, r3` is not taken
(`g13+0x148` is not negative) and `bbc 3, g6` skips the first call site.

Warm path:

```text
ldt g7+0x1f4  (x,y,z both 0)
ldt g8+0x1f4  (x,y,z both 0)
r12 = r4 - r8 = 0
r13 = 0
r14 = r6 - r10 = 0
st 0x18003030 -> FIFO (g11)[g12]   # command word
st r12        -> FIFO
st r13        -> FIFO
st r14        -> FIFO
ld FIFO -> r12
ld FIFO -> r13
ld FIFO -> r14
st r12 -> g13+0xc8
st r13 -> g13+0xcc
st r14 -> g13+0xd0
ret -> 0x236b8
```

FIFO window measured at `0x00884000` for all four writes and three reads.
All fighter `+0x1f4` components are 0, so the three FIFO reads and the
three `g13` stores are all 0. Command word is the literal `0x18003030`.

### Verdict

**GO.** Deterministic from measured register/memory inputs. Recover via
the same `vf2_model2a_write_u32` / `read_u32` sequence so the Model 2A
observer sees identical FIFO accesses. Fail-closed if `+0x1f4` components
are non-zero **or** if we later prove the FIFO reply is not a pure
function of the four writes — until then, implement the measured read
values as whatever the hardware returns (do not hardcode zeros as the
semantic result; read them back).

## Recovery order after this note

1. **v0284** — `0x233d0` + `0x2364c` (T2). Small, single-path, high
   confidence. Wire into `hybrid_execute_coli_body`:
   - after the six `0x23878` walk, stop at `0x233d0`, native, resume
     `0x235a0 → 0x238a4`;
   - after `0x238f8`, stop at `0x2364c`, native, resume
     `0x236b8 → 0x22210`.
2. **v0285** — `0x2396c` warm path (T3). Replace the six-step bitremap
   walk with two native entries. Keep `field_xxx` / raw offsets.
3. Shell `0x23524` (179) only after the children are native.

Pins that must not move: whole-task **9214 / 18 / 19**, corridor
**320/320 MATCH**.
