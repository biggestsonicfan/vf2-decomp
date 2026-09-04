# game_info 0x18644 positive base 0x8140 bit-21 low variants — v0247

## Scope

- Base `0x8140` (bits 15+8+6) with mandatory high bit 21 (`0x00200000`)
  plus any of the 16 outer-high combinations over 26/29/30/31 already
  admitted bare via v0176/v0226/v0227 (`0000,0400,2000,2400,4000,4400,
  6000,6400,8000,8400,A000,A400,C000,C400,E000,E400` × `0x00200000`).
- Low variants add any non-empty subset of bits 1,2,4
  (`0x02,0x04,0x10` and combos `0x06,0x12,0x14,0x16`): 7 lows.
- Family: 16 outers x 7 lows = 112 masks, each a 36-case full-dispatch
  matrix (3 distributions x countdown 0/1 x mode-bit-6 0/1 x thresholds
  0..2) = 4032 ROM-backed cases.

## Pre-fix measurement

- Every mask in the family was `0/36 exact` with a uniform signature:
  bilateral `dins=+5`, unilateral `dins=+3`; `cd0` ref EQUAL vs native
  EQUAL-after-fix, `cd1` ref LESS vs native EQUAL-before-fix.
- No stale-frame or memory effect: post-fix pairs differ only in the one
  condition byte plus the instruction delta (same shape as the v0226
  high-pair blocks at `hybrid.c:14825-14835`).
- Bare masks (`has_low == 0`) stay on their existing v0176/exact
  admissions — disjoint predicate, no overlap.
- Out-of-scope control: `0x0420C142` (base `0xC140` family, bit 21 + low)
  remains `0/36` after this change — correctly still fail-closed.

## Recovery (v0247)

- One predicate in `src/recovered/hybrid.c` before the v0175 block:
  `fighter0_state == 8 && fighter1_state == 8 && measured_matrix_distribution
  && threshold >= 0 && (combined & 0x00200000) != 0 && (combined & 0x16) != 0
  && (combined & ~0xE4200016) == 0x00008140`.
- Effect: `native_instructions -= bilateral ? 5 : 3`;
  `hybrid_set_compare_result(countdown ? LESS : EQUAL)`;
  `hybrid_set_stale_low(cpu, f0, bl)` — identical accounting to the
  neighbouring high-pair low-variant blocks.
- First version omitted `hybrid_set_stale_low` and stayed `0/36`
  (arch DIFF on the stale frame); adding it flipped the sampled masks
  to `36/36`.

## Coverage

- 112 masks `36/36 exact` via
  `decomp/i960/tools/validate_game_info_full_dispatch.py`
  (`--base out/bit21_8140/keep/game_info_1645c.vf2snap --workers 6`).
- Grand total `1111 -> 1223`.
