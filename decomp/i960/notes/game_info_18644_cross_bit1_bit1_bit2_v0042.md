# fa_game_info 0x18644 — exact 0x102 ↔ 0x106 recovery (v0042)

## Scope

This note records ROM-backed recovery of the exact ordered pair family:

- `0x102 -> 0x106`
- `0x106 -> 0x102`

where `0x106` is `0x102` plus state bit 2. The pair belongs to instruction-vector class 9:

`(232, 237, 208, 213)` for `(cd0/m0, cd0/m1, cd1/m0, cd1/m1)`.

No other unvalidated state pair is admitted by this change.

## Semantic opening

The C implementation previously rejected this pair at the bilateral composition guard and the bit1-specific composition guard. The candidate opened only the exact `state8+bit1 ↔ state8+bit1+bit2` composition at those two fail-closed boundaries.

The semantic-only candidate was ROM-compared separately at both child call sites. All 16 isolated probes had matching architectural state, mutable memory, calls, returns, interrupt entries and interrupt returns. Only instruction accounting differed.

## Measured instruction accounting

Both orientations produced the same correction table.

First child, return `0x164b0`:

- countdown 0, mode6 0: `+9`
- countdown 0, mode6 1: `+9`
- countdown 1, mode6 0: `+11`
- countdown 1, mode6 1: `+11`

Second child, return `0x164c4`:

- countdown 0, mode6 0: `+5`
- countdown 0, mode6 1: `+6`
- countdown 1, mode6 0: `+10`
- countdown 1, mode6 1: `+10`

Unlike several previously recovered asymmetric families, this pair requires no orientation-specific role swap in the accounting rule.

## Exact validation

Final ROM-validated functional candidate:

`affe60d1d95cfc8daf4a0f5c07ab2267858fe891`

Validation results:

- isolated child probes: **16/16 exact**
- full chained execution (`C child #1 -> ROM caller -> C child #2 -> ROM tail`): **8/8 exact**

For every chained case:

- snapshot: MATCH
- instructions delta: 0
- procedure calls delta: 0
- procedure returns delta: 0
- interrupt entries delta: 0
- interrupt returns delta: 0

## Tooling improvement

During this recovery the fragile hand-written unified-diff staging path was replaced by the reusable `decomp/i960/tools/apply_exact_edits.py` helper. Edit specifications are declarative and fail closed unless each source snippet occurs exactly once. The pair-specific staging specifications were removed after promotion; the reusable helper remains in the repository.

## Coverage after promotion

The state matrix now has **28/64 exact ordered pairs** individually ROM-validated. All **9/9 instruction-vector classes** continue to have at least one recovered representative. Unvalidated pairs remain explicit fail-closed boundaries.
