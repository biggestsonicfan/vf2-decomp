# fa_coli `0x23524` shell warm path — v0286

Measurement-only slice. Extracts the exact 179-instruction shell path
that remains interpreted inside `0x23524` after v0285 (all callees are
already native). Decides go/no-go for the native shell recovery (v0287).

## Drive

Existing warm trace from `out/coli-fail.vf2snap` (parked at `0x221e8`,
flags `0x8a00`):

```text
vf2probe --snapshot out/coli-fail.vf2snap \
  --until 0x00022210 --max-steps 50000 --trace --memory-trace
```

- **9,158** instructions, **14** procedure calls / **14** returns
- final `ip = 0x22210`
- shell own steps: **179** (including the final `ret`)
- shell writes: **53** (all size 4)

## Function shape

`vf2i960 function` reports `sub_00023524: end=0x0002364c blocks=6`.
The warm path also enters `0x23694` via `bal` and returns through
`bx (g14)` to `0x23648`, then `ret` to `0x22210`.

## Warm path (glue-by-glue)

### Prologue — 14 insns, 6 FIFO writes

| ip | mnemonic | effect |
|----|----------|--------|
| `0x23524` | `ldt 0x1f4(g7), r4` | fighter0 pos triple |
| `0x23528` | `ldt 0x1f4(g8), r8` | fighter1 pos triple |
| `0x2352c` | `subr r4, r8, r12` | float delta (warm 0) |
| `0x23530` | `subr r6, r10, r14` | float delta (warm 0) |
| `0x23534` | `lda 0x1f003e3e, r15` | FIFO command |
| `0x2353c..0x23550` | `st` ×6 | FIFO `0x884000`: cmd + 5 words |
| `0x23554` | `lda 0x40(g13), r14` | clear dest |
| `0x23558` | `mov 0, r15` | clear value |
| `0x2355c` | `mov 16, r13` | trip count |

Warm FIFO payload: `0x1f003e3e, 0, 0, 0, 0, 0`.

### Clear loop — 64 insns, 16 writes

`0x23560..0x2356c` ×16: `st 0` into `g13+0x40 .. g13+0x7c`
(`cmpdeco`/`bl`). Warm `g13 = 0x514980` → `0x5149c0..0x5149fc`.

### Call glue — 12 insns, 1 FIFO write

| ip | effect |
|----|--------|
| `0x23570` | `lda 0x1d003a3a, r15` |
| `0x23578` | `st` FIFO command `0x1d003a3a` |
| `0x2357c` | `ld 0x500804, g7` (fighter0) |
| `0x23584` | `lda 0x01000550, g9` |
| `0x2358c` | `call 0x2396c` inv0 |
| `0x23590` | `ld 0x500808, g7` (fighter1) |
| `0x23598` | `call 0x2396c` inv1 |

### Flag builder + gate — 3 insns

| ip | effect |
|----|--------|
| `0x2359c` | `call 0x233d0` → leaves `g6 = 0` |
| `0x235a0` | `bbc 0, g6, 0x235ac` — warm **taken** |

Sibling `0x235a4` (`call 0x2364c; b 0x23648`) is the g6 bit-0-set path.
**Fail closed.**

### g3-scan pair — 6 insns

| ip | effect |
|----|--------|
| `0x235ac` | `ld 0x500804, g7` |
| `0x235b4` | `call 0x238a4` inv0 → `g3 = 0` |
| `0x235b8` | `mov g3, r11` (`r11 = 0`) |
| `0x235bc` | `ld 0x500808, g7` |
| `0x235c4` | `call 0x238a4` inv1 → `g3 = 0` |
| `0x235c8` | `mov g3, r12` (`r12 = 0`) |

### Window / cluster push — 22 insns, 8 writes

| ip | effect |
|----|--------|
| `0x235cc..0x235d4` | `ld g13+0x88 / +0xb4 / +0xb8` |
| `0x235d8` | `lda 0x1d803b3b, r15` |
| `0x235e0..0x235f4` | `st` ×6 FIFO: `0x1d803b3b` + 5 words |
| `0x235f8..0x23604` | `ld` ×4 FIFO replies → `r12, r13, g4, g5` (warm 0) |
| `0x23608` | `st r12, 0x144(g13)` |
| `0x2360c` | `st r13, 0x148(g13)` |

### Rolling average — 10 insns, 3 writes

| ip | effect |
|----|--------|
| `0x23610..0x23618` | `ld +0xf4 / +0xf0`, `addr` |
| `0x2361c` | `st` old `+0xf0` → `+0xf4` |
| `0x23620..0x23624` | `ld +0xec`, `addr` |
| `0x23628` | `st` old `+0xec` → `+0xf0` |
| `0x2362c..0x23630` | `addr g5`, `st g5 → +0xec` |
| `0x23634` | `lda 0x40800000, r14` (4.0f) |
| `0x2363c` | `divr r14, r15, g5` |

Warm all-zero → all three stores are 0, `g5 = 0`.

### Nested scan + bal — 2 insns

| ip | effect |
|----|--------|
| `0x23640` | `call 0x238f8` (warm no-op, 2855 insns) |
| `0x23644` | `bal 0x23694` (return addr `0x23648`) |

### `0x23694` warm path — 46 insns, 14 writes

| ip | effect |
|----|--------|
| `0x23694` | `ld 0x500804, g7` |
| `0x2369c` | `ld 0x500808, g8` |
| `0x236a4` | `bbc 3, g6, 0x236ac` — warm **taken** (bit 3 clear) |
| `0x236ac` | `ld 0x148(g13), r3` (warm 0) |
| `0x236b0` | `cmpobl 0, r3, 0x236c4` — warm **not taken** |
| `0x236b4` | `call 0x2364c` |
| `0x236b8` | `movt 0, r8` |
| `0x236bc` | `movt 0, r12` |
| `0x236c0` | `b 0x237b0` |
| `0x237b0..0x237c4` | `st` ×6 → `g13+0xd4..0xe8` (warm 0) |
| `0x237c8..0x237e4` | fighter `+0x644/+0x64c` load + `subr` deltas |
| `0x237e8` | `lda 0x1e803d3d, r15` |
| `0x237f0..0x23808` | `st` ×7 FIFO: `0x1e803d3d` + 6 words |
| `0x2380c..0x23820` | fighter0 `+0x18/+0x20` load/addr/`st` |
| `0x23824..0x23838` | fighter1 `+0x18/+0x20` load/addr/`st` |
| `0x2383c..0x23854` | f0 `+0x650` threshold `0x3cf5c28f` (0.03f), clamp |
| `0x23858..0x23870` | f1 `+0x650` threshold, clamp |
| `0x23874` | `bx (g14)` → `0x23648` |
| `0x23648` | `ret` → `0x22210` |

Sibling paths inside `0x23694` that are **not** warm:

- `bbc 3, g6` taken → `call 0x2364c` at `0x236a8` (skipped)
- `cmpobl 0, r3` taken → `0x236c4` threshold/mulr path (skipped)
- `cmpobne 5/4, r15` at `0x236c8`/`0x23700` (not reached)

**All fail closed.**

## Write map (warm)

| address | g13-rel / fighter-rel | value | source |
|---------|----------------------|-------|--------|
| `0x884000` ×6 | FIFO | `0x1f003e3e` + 5×0 | prologue |
| `g13+0x40..0x7c` | 16 words | 0 | clear loop |
| `0x884000` | FIFO | `0x1d003a3a` | pre-0x2396c |
| `0x884000` ×6 | FIFO | `0x1d803b3b` + 5×0 | window push |
| `g13+0x144` | | 0 | FIFO reply |
| `g13+0x148` | | 0 | FIFO reply |
| `g13+0xf4` | | 0 | rolling |
| `g13+0xf0` | | 0 | rolling |
| `g13+0xec` | | 0 | rolling |
| `g13+0xd4..0xe8` | 6 words | 0 | post-0x2364c |
| `0x884000` ×7 | FIFO | `0x1e803d3d` + 6×0 | delta push |
| `f0+0x198`, `f0+0x1a0` | `+0x18`, `+0x20` | 0 | pos update |
| `f1+0x198`, `f1+0x1a0` | `+0x18`, `+0x20` | 0 | pos update |
| `f0+0x750` | `+0x650` | 0 | threshold clamp |
| `f1+0x750` | `+0x650` | 0 | threshold clamp |

`g13 = 0x514980`, `f0 = 0x510800`, `f1 = 0x512800` on this drive.

## Accounting for native `0x23524`

| component | insns | calls | returns |
|-----------|------:|------:|--------:|
| shell own excl ret | 178 | 7 | 0 |
| `0x2396c` ×2 (incl remaps) | 6046 | 6 | 8 |
| `0x233d0` | 44 | 0 | 1 |
| `0x238a4` ×2 | 10 | 0 | 2 |
| `0x238f8` | 2855 | 0 | 1 |
| `0x2364c` | 17 | 0 | 1 |
| shell `ret` | 1 | 0 | 1 |
| **total** | **9151** | **13** | **14** |

`hybrid_complete_procedure(body=9150, nested_calls=13, nested_returns=13)`
plus the completed ret yields 9151 / 13 / 14 for the procedure.
Caller `0x221e8` adds 7 insns + 1 call → path total **9158 / 14 / 14**.

## Verdict

**GO.** The warm path is a single measured sequence. Only unmeasured
branches are the documented siblings above; they fail closed.

## Proof

- Guest trace 9158 steps; shell subset 179 steps / 53 writes
- All warm FIFO payloads and g13/fighter stores decoded from `bytes`
- Call sequence matches v0278 attribution
- No snapshot/trace/ROM data is committed
