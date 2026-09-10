# fa_player next targets after coli closure — v0292

## Context

The `fa_coli` warm path is fully native (v0289–v0290). The next
high-value gameplay frontier is `fa_player` / geometry / physics, per
`AGENTS.md` and `docs/UNCOVERED_BRANCHES.md` §2.

This note records the **measured next boundaries** so a future slice
starts from evidence rather than guesswork. No C recovery in this
commit.

## Current accepted player corridor

`hybrid_first_dispatch_task_execute` walks a deep native chain for
`fa_player` entry `0x13f08`:

```
prefix → 0x19ef8 → 0x1428c → 0x142c0 → 0x14310
→ 0x143e4 → 0x1ab74 → 0x27ce0 → 0x27d00
→ 0x28184 / 0x28780 / 0x28268
→ 0x27d90 / 0x27dcc / 0x27fa0 / 0x2901c
→ 0x28174 → 0x29414 → post-29414
```

When a native step returns `VF2_ERROR_UNSUPPORTED`, the dispatcher
falls back to `hybrid_execute_interpreted_task` from that boundary.
Later player branches remain original-i960 continuations.

## High-value missing systems (from UNCOVERED_BRANCHES)

- fighter physics
- hitboxes / hurtboxes
- collision (beyond the coli shell already recovered)
- damage and combos
- ring-out and arena-boundary handling
- CPU opponent decision logic

These are **not** compact leaves. Each needs a new measured drive,
a witness snapshot, and a differential pin before any C recovery.

## Recommended next measured targets (in value order)

### 1. Player state-selector branches after `0x19ef8`

`hybrid_execute_player_19ef8` already covers the accepted profile and
fails closed on unmeasured `player_flags` bits (5, 6, 21, 23) and on
`player_state_flags != 0`. Forcing those bits from a sixth-entry or
PUNCH-adjacent snapshot yields new sibling witnesses without leaving
the already-native corridor shape.

Drive sketch:

```text
vf2probe --snapshot <sixth-or-player-boundary> \
  --set-u32 <fighter>+0x1a4=<measured-mask> \
  --until 0x0001428c --trace --memory-trace
```

### 2. `0x29414` non-zero path

`hybrid_execute_player_29414_zero_path` covers the measured zero
outcome. A drive that makes the pre-`0x29414` predicate non-zero opens
the sibling. Measure first; recover only if the path is compact.

### 3. Geometry helpers after `0x28780`

The `0x28780` geometry body is native. Downstream helpers called from
its tail are the natural place to grow the player corridor without
touching physics. Rank candidates with `frontier.py` on a
`--memory-trace` of the current player boundary.

### 4. Physics / hitboxes (large)

Do **not** start here. Requires a dedicated input-driven witness
(different PUNCH-like pin), candidate field inference via
`infer_structs.py`, and a new differential contract. Open only after
the selector/geometry slices above are closed.

## Tooling available

- `tools/python/frontier.py` — ranks guest edges from traces/corpora;
  supports `--exclude-recovered`, `--duckdb`, `--parquet`. Validated
  on `out/coli-midbody-v0289-full.jsonl` (56 steps, 22 memory events).
- `tools/python/taint.py` — targeted dynamic taint over loads, bitops,
  shifts, cmp and conditional branches; sideband only.
- `tools/python/infer_structs.py` — candidate fighter field offsets
  from repeated `base+offset` accesses.

## Proof

- No C recovery and no pin change in this commit.
- PUNCH remains `320/320` MATCH from v0290.
- No snapshot/trace/ROM data is committed.
