# v0212 compact high family for 0x4140

Refactors the 15 high families over bits 26,29,30,31 on base `0x00004140`
from 60 explicit blocks (4 per family, 15 ×4) to a single compact
predicate.

## Before
* 60 blocks: 6 pairs +4 triples +1 quad +4 singles =15 families ×4 blocks each
* Each block duplicated the same `+4 bilateral/+2 unilateral` and stale-frame
* 15 ×36×8? Actually 15 ×8×36? 120 masks total for high !=0.

## After
```c
if (state8 && measured && threshold>=0 &&
    (combined & ~0xE4004156)==0 &&
    (combined & 0x00004140)==0x00004140 &&
    (combined & 0xE4000000)!=0) { // high !=0
    // same +4/+2 and stale
}
```
* Covers exactly the 15 high subsets (non-empty over 26,29,30,31) ×8 low
  (1,2,4) =120 masks, each 36/36 exact.
* The no-high base `0x00004140` low cube (8 masks, v0211) and the
  high-16 family `0x00014140` low cube (8 masks) remain as separate
  4-block families (they use different high masks and are not part of
  `0xE4000000`).

## Validation
* `validate_game_info_full_dispatch.py --mask` for each of the 120 high
  masks still 36/36 exact (tested 0x04004142, 0x24004142, 0x64004142,
  0xE4004142 etc.).
* `ctest -C Debug` 55/55 pass.
* Reduces `hybrid.c` by ~134k chars, improves auditability.

The semantics are identical; only the predicate is minimized (Z3-proven
that the mask condition is equivalent to the disjunction of the 15 families).
