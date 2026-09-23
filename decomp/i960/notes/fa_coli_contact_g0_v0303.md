# v0303: `0x22404` contact sibling with non-empty scan (`g0 = 1`)

## Verdict

`coli_22404_body` now natively recovers the measured bit-8-set path
that leaves a **non-empty** mask after `andnot` with `g8+0x6dc` and
returns `g0 = 1` (the polygon-FIFO contact). This is the path that
previously failed closed and is required to reach the `0x225cc`
resolver.

Measured drive (v0282 recipe, re-run v0303):

```text
vf2probe --snapshot out/coli-22404-e1.vf2snap \
  --set-u32 0x00510b24=0x100 \
  --set-u8  0x005111a0=0x01 \
  --set-u32 0x005149cc=0xffff \
  --until 0x0002222c --max-steps 800 --trace --memory-trace
```

- **73 instructions** (body 72 + ret), 0 counted calls, 1 return
- `g0 = 1`, `g9 = 0x01000550`
- pending bit set in `g13+0x90`
- result `0xffff` stored at `g8+0x6d4`

## CFG (slot 0)

Common prologue (6): slot, old snap, new snap, store, flags, `bbs 8`.

Bit-8-set:

1. equal snapshots (`cmpobe`); pending bit clear; `thr_a >= thr_b`;
   helper `0x223bc` returns 0 (`g7+0x5b8` bit 0 clear)
2. `mask = *(u32*)(0x02007aca + index*4)` with `index = g7+0x820`
   (measured index 1 → value **8**)
3. slot-0 scan loop: `scanbit` (MSB-first, as in `executor.c`), on hit
   `acc |= *(u32*)(g13+0x40+bit*4)`, `clrbit`, repeat; on miss `bno`
   to `0x224b4` with `r5 = 31`
4. `result = acc & ~*(u16*)(g8+0x6dc)`; store to `g8+0x6d4`
5. empty → `mov 0,g0` / ret (v0290 sibling, body 29 for immediate miss)
6. non-empty → pending `setbit slot`, `g0 = 1`, polygon FIFO via
   `(g11)[g12]`, `stt` triple into `g8+0x65c`, `g9 = 0x01000550`

## Guards kept fail-closed

- slot 1 (16-trip `bbc`/`setbit` scan loop)
- `g8+0x26 != 0` (FIFO cursor path at `0x22518`)
- unequal snapshots, pending bit set, `thr_a < thr_b`, helper `!= 0`
- `0x225cc` resolver body (caller mid-body tail still requires both
  contact results zero)

## Accounting

Body count is dynamic (scan-loop trips). Measured one-hit shape:

```text
22 prefix + 8 scan + 4 andnot + 4 pending/g0
+ 3 fifo-sel + 2 ldt/b + 4 fifo-prefix + 2 g8+26
+ 2 skip-cursor + 9 coords + 2 hdr + 3 st-triple
+ 3 ld-port + 2 tail-hdr + 1 stt + 1 g9 = 72
```

Immediate miss (table[0] = 0) remains body 29 (v0290 pin).

## Memory notes

- Model buffer is **little-endian** (`vf2_model2a_read_u32`).
- `0x02007aca` is **main-data** (base `0x02000000`), not main ROM.
- Command port = `g11 + g12` (measured `0x00880000+0x00004000`).
- `scanbit` dest on miss is 31 (`executor.c`).

## Proof

- unit test `test_coli_contact_query_22404_early_path` pins
  `73 / 0 / 1`, `g0 = 1`, `g9`, pending bit, `g8+0x6d4 = 0xffff`;
  slot 1 fails closed
- ctest Debug **56/56**
- PUNCH **320/320 MATCH** / 12,946 blocks / 14,962,620 insns
- input-17 **64/64 MATCH** / 2,368 blocks / 2,428,988 insns

## Remaining

- `0x225cc` resolver (needs live caller path after non-zero contact)
- slot-1 scan loop; `g8+0x26 != 0` FIFO cursor
- mid-body tail still fails closed when either contact returns non-zero
