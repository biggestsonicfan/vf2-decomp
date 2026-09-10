# fa_coli bit-8-set compact siblings — v0290

## Summary

Admits two measured compact siblings of the coli mid-body children:

1. `0x22298` bits 8 and 1 set — 8 instructions, same `stos 0` store
2. `0x22404` bit 8 set with equal snapshots / empty scan — 30 instructions,
   store 0 into `g8+0x6d4`, `g0 = 0`

Other bit-8-set sub-branches remain explicit boundaries.

## Measurement

### `0x22298` bits 8+1 set

From `out/coli-midbody-e1.vf2snap`, `--set-u32 0x00512b24=0x102`
(fighter1 `+0x1a4` bits 8 and 1):

```text
8 instructions, 0 calls, 1 return, exit 0x22214
mov/ld/ld/ldob → bbc 8 not taken → bbs 1 taken → stos 0 → ret
```

Same memory store as the warm path (`g7+0x6dc = 0`).

### `0x22404` bit 8 set, 30-insn shape

From `out/coli-22404-e1.vf2snap`, `--set-u32 0x00510b24=0x100`:

```text
30 instructions, 0 counted calls, 1 return, exit 0x2222c
```

Preconditions (all measured on this drive):

| check | value |
|-------|-------|
| `g7+0x1a4` bit 8 | set |
| `g13+0x8c[slot]` vs `g7+0x1a8` | equal |
| `g13+0x90` slot bit | clear |
| `g7+0x1aa` vs `g7+0x808` | `a >= b` |
| `g7+0x5b8` bit 0 | clear → helper `0x223bc` returns `r3 = 0` |
| `g7+0x820` | 0 → ROM table lookup 0 → `scanbit` NoBit |

Effects: snapshot store (prologue), `stos 0` into `g8+0x6d4`,
`g0 = 0`, `g14 = 0x2244c`.

## Recovery

Body-only statics now return an instruction count:

- `coli_22298_body` — warm body 6, sibling body 7
- `coli_22404_body` — warm body 13, sibling body 29

Standalone exports wrap the bodies with `hybrid_complete_procedure`.
`coli_midbody_tail_execute` sums children dynamically.

The original `0x22404` always stores the snapshot **before** the bit-8
check; the recovery matches that order, so a failing sibling still
leaves the new snapshot in the slot.

## Proof

- Unit tests: warm paths unchanged; bit8+bit1 `0x22298` pins `8 / 0 / 1`;
  bit8 `0x22404` sibling pins `30 / 0 / 1`, `g0 = 0`, `g14 = 0x2244c`;
  unequal-snapshot sibling fails closed after the snapshot store.
- `ctest -C Debug` (run after commit)
- PUNCH: **320/320 MATCH** / `14,962,620` instructions
- No snapshot/trace/ROM data is committed

## Remaining coli frontier

- `0x22298` bit-8-set without bit 1 (16-trip float compare loops)
- `0x22404` bit-8-set with non-empty scan mask / `g0 = 1` path
- `0x225cc` resolver (needs a non-zero contact result)
