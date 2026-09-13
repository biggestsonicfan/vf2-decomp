# v0345-B: site-A full leg runs natively end-to-end (fa_coli done)

## Verdict

`fa_coli` is finished for every measured drive: the site-A leg
(entry → cascade → `0x502a4`#siteA → continuation → site-B prefix →
`0x502a4`#siteB → existing `0x22e24` tail → OK) executes natively
with exact counts (883 steps), exact calls/rets (7/8) and exact
stores. The wrapper unit proves it. Remaining `fa_coli` items are
unmeasured-input variants (other flag/scan/board combos still fail
closed by design), not missing code on this leg.

## What was built

- `coli_225cc_sitea_cont` (new static body): `0x22960` → `0x22e24`
  caller-frame spans — `0x7fc0`#1, `0x22964` inline, balx `0x9444`,
  `0x7fc0`#2, `0x9450` scan tail (`cmpobl` taken iff `r15 < g0`),
  `0x2298c` join, `0x7fc0`#3, site-B prefix, `0x502a4`#siteB,
  `0x7fc0`#4. Pack locals pass through untouched; `g0` threads
  through; caller-frame `r3 == 1` is a loud documented assumption
  (untracked, single use).
- Join into the existing `0x22e24` tail via `coli_22e24_join`
  (`goto` precedent: `bit13_skip`). The tail (230d4-long,
  23238, 1ab34-miss, float/FIFO, diagnostic stores) needed zero
  changes — state arrives identical (same pack, `g0 = 4` reset).
- `long_body` reports calls/rets (legacy 4/4 default, site-A
  overwrites 7/7). Wrapper units unchanged otherwise.

## Key findings

- The drive takes the `0x22edc` branch (1ab34-miss returns `g0 =
  0): the `g0`-record parser is skipped, not modeled.
- `cmpobl r15,g0` = taken iff `r15 < g0` (unsigned): 4 taken
  (`0x20202020`) + nt (`0x2020`). Another src1/src2-order trap,
  measured not guessed.
- Observer hex is LE-concatenated bytes: trace `"0c74dac8"` =
  value `0xC8DA740C` (L2's assert had it right); same trap as the
  v0344 `r3 = 1` (not `0x1000000`) read. Always decode bytes,
  never read the hex string as the value.
- `+0x6d8` u32 `0x101` is live-correct (cascade byte0 +
  prefix byte1); asserting `== 1` was the bug, not the code.
- `g9` table consts per site (`0x7de` → `0x85e` → `0x6e8` →
  `0x85e`); `g11 = FIFO`, `g12 = 0` (every record @ `0x884000`).
- Trace-skimming discipline: verify every branch by comparing
  `ip_after` against both successors mechanically (the false
  `ldob`-preserves-upper and `cmpobne`-taken scares this turn came
  from misread prints; fresh probes settled both in favor of the
  existing code).

## Unit (v0345-B shape in `test_coli_225cc_long`)

OK, `ip == 0x22240`, delta 883, calls 7, rets 8; stores:
`0x503200 = 0x6d6f6320`, `+0x6d8 = 0x101`, `+0x6d9 = 1`,
`+0x198 = 0x0c0004ac`, `g7+0x194 = 0x14000001`, `+0x700 = 2`,
`+0x5de = 0xfff2`, `+0x5e4/0x5e0/0x5e8 = 0xc8da740c`.

## Pre-existing gaps (not this slice, shared by every shape)

- The `0x230b8`/`0x22294` rets stay unmodeled: ip lands on the
  entered return (`0x22240`), not the live scheduler target
  (`0x10dcc`). Fixing that is cross-cutting wrapper-completion
  work (double pop), identical for all shapes.
- Site-B-only entry (its own gate) still fail-closes: its prefix
  is proven only as part of this leg.
- Unmeasured flag/scan/board combos fail closed throughout.
