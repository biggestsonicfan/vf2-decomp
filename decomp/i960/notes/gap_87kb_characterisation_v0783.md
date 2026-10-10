# v0783: Phase 1.3 — the 87,284 B gap at 0x4ec00..0x640f4 characterised

**advance_plan_v0764.md Slice 6.** The largest unknown region per
`completion_plan_v0734.md` (87,284 B between `tile_controller_update`'s end at
`0x4ec00` and `task_osage`'s start at `0x640f4`). **No `src/` change, no
`functions.csv` change** — research only.

## Method (and why it supersedes v0734k for this gap)

v0734h/v0734k characterised the four *smaller* gap runs by hand-probing
`vf2i960 function` at sparse offsets. This slice instead runs the full-image
CFG sweep once:

```sh
build\Debug\vf2i960.exe analyze roms\vf2 out\analysis
```

which produces `out/analysis/functions.csv` (263 function records with
measured extents), `out/analysis/xrefs.csv` (call/address edges) and
`out/analysis/i960.asm` (the reachable linear listing) — then overlays
`decomp/i960/original_symbols.csv` (the 301 shipped Sega symbol names, PR
#120). The overlay is the point of this slice: the gap is not anonymous.

## What is inside the gap

### Shipped-symbol anchors (24 records inside the gap)

The attract/demo and attract-select machinery:

| address | symbol | measured extent (analyze) |
|---|---|---|
| 0x51d3c | `sampling_init` | 0x51d3c..0x51f98 — 604 B |
| 0x51e7c | `sampling` | interior label inside sampling_init |
| 0x51f98 | `hit_and_guard_check` | 0x51f98..0x520d0 — 312 B |
| 0x520d0 | `make_action_table` | 0x520d0..0x52378 — 680 B |
| 0x52378 | `recog_sample` | 0x52378..0x52518 — 416 B, interior loop labels 0x523b0..0x524d8 |
| 0x52974 | `rank_check` | 0x52974..0x52bc8 — 596 B |
| 0x52e7c | `make_personal_table` | 0x52e7c..0x52f64 — 232 B |
| 0x53884 | `reject_sample` | 0x53884..0x538e4 — 96 B |
| 0x53cb8 | `name_entry` | **no analyzer extent** — no CFG root reaches it |
| 0x61684 | `rob_kage_set_flag` | 0x61684..0x61a14 — 912 B |
| 0x61b88 | `rob_kage_disp_outside` | 0x61b88..0x61ba0 — 24 B |
| 0x62fac | `hat_off_disp` | 0x62fac..0x63800 — 2,132 B |

Already recovered under these names or provisional ones (the rename slice
retiring the provisional ones is its own piece of work):

| address | symbol | status |
|---|---|---|
| 0x53750 | `display_profile_load` | recovered v0780 (name already correct) |
| 0x5427c | `poly_record_load` | recovered v0776 under the shipped name |
| 0x5a460 | `color_table_snapshot` | recovered v0778 (provisional; no shipped record) |
| 0x6010c | `sram_clear_for_game_assign` | recovered v0781 as `assignment_init` — **contradicts** shipped name |
| 0x60330 | `sram_clear_for_coin_assign` | recovered v0774 as `poly_record_clear` — **contradicts** shipped name |

### Unnamed entries (analyzer-discovered, `sub_000XXXXX`)

`sub_00050130` (44 B), `sub_00053810` (116 B), `sub_00056ee0` (440 B),
`sub_00057bdc` (132 B), `sub_00052518` (516 B), `sub_0005271c` (88 B),
`sub_00052774` (216 B), `sub_0005284c` (168 B), `sub_00061a14` (176 B),
`sub_00061ac4` (196 B), `sub_00061ba0` (348 B), `sub_0004f904` (64 B).

`sub_00061ba0` is `rob_kage_disp_outside`'s callee (called with
`g4 = *(g7 + 0x684)`, `g5 = 2` when `ldob(g7 + 0x2a) == 0`).

### Coverage arithmetic (honest, not laundered)

Measured extents inside the gap: **9,720 B — 11.1% of the gap.** The remainder
(~77.6 KB) is outside the analyzer's reachability roots. "No analyzer extent"
is not "not code": the CFG sweep only follows task-descriptor/vector roots, so
regions reached exclusively by indirect dispatch (or data tables, or the
sample-stream input tables the `sampling` chain presumably consumes) simply
do not appear. This is the same lower-bound caveat the completion plan carries
for all gap arithmetic.

### Xref anchors (who drives the gap)

- `sampling_init` chain: called from `0x51exx` bodies (`hit_and_guard_check`
  from 0x51ea8/0x51f34, `make_action_table` from 0x51ecc/0x51f58,
  `rank_check` from 0x51ec4/0x51f50, `make_personal_table` from
  0x51ed8/0x51f64 — a straight-line sequence inside `sampling` itself).
- `recog_sample` and `reject_sample` are called from inside
  `make_action_table` (0x52198/0x5219c).
- `rob_kage_set_flag` from `0x1de60`; `rob_kage_disp_outside` +
  `hat_off_disp` from six sites across `0x1dec4..0x1edbc` — the
  camera/display task family (attract-mode and character-portrait
  control).

### The select corridor sits immediately below the gap, not inside it

Adjacent evidence worth pinning here because it re-scopes v0764's Slice 3
("post-cf04 sibling cascade") away from guessing at clrbit siblings:

- `select_init` `0x32090..0x324a0` (36 blocks, no indirect flow).
  - Entry: bit 0 of `*(g13)` set → path B; clear → path A.
  - Path A: clrbit 7/clrbit 1/setbit 6 of `*(g13)`, store `0x320cc` to
    `*(g13 + 0xc)` (next state handler), ret.
  - `0x320cc` is a 3-word spin stub (`ld (g13), r15; bbs 0, r15, 0x320d8;
    ret`) — re-tests the same bit and joins path B once set.
  - Path B: selector byte `ldob(g13 + 4)` picks sides (0 vs 1 mirrors
    `0x500804`/`0x500808` fighters, `0x500840`/`0x50083c` records,
    `0x500868`/`0x50086c` task slots, clrbit 18 vs 19 of `0x500068`),
    calls `0x4b5b0` (clears 3 shorts at `g7 + 0x6a8/0x6aa/0x6b0`, then
    `0x4b570`: clears 2 shorts at `0x5501ca/0x55020a` or
    `0x5501ea/0x55022a` depending on `ldob(g7 + 4)`) and recovered-`0xcf04`
    (v0747, inline-able by v0756 discipline), then stores `select_pl`
    (`0x324a0`) at `*(g13 + 0xc)` and `task_osage` (`0x640f4`) at the
    side's `*(0x500868`/`0x50086c)` task-record `+0xc`.
  - The v0756/v0756b recoveries `0x32284`/`0x323fc` are **interior blocks
    of `select_init`** (the two mirrored side-tail exits), not siblings of
    anything. The shipped symbol table and the extents agree on this.
- `select_pl` `0x324a0..0x334a8` (161 blocks) and `sel_pl_p2`
  `0x324d0..0x334a8` (79 blocks) share the epilogue `ret` at `0x334a4` — the
  v0734a shared-epilogue pattern, here the P1/P2 select handlers.

## What this slice does NOT do

- No `functions.csv` rows added for the `sub_*` entries (naming wants a second
  independent signal per the v0734a two-source rule).
- No recovery of the sampling chain / rob_kage / hat_off bodies — those are
  real slices, now with shipped names as the scaffold.
- No closure of the completion plan's 1.3 total (other gaps unchanged).

## Validated

- All extents above from `vf2i960 analyze` (`out/analysis/functions.csv`) or
  `vf2i960 function` / `vf2i960 disasm` against `roms/vf2`.
- All names cross-checked against `decomp/i960/original_symbols.csv`
  (records: `sampling_init` 0x51d3c … `hat_off_disp` 0x62fac,
  `name_entry` "record 285", `sram_clear_for_game_assign` 286,
  `sram_clear_for_coin_assign` 287).
- `git diff --stat src` and `decomp/i960/functions.csv`: both empty.
