# Candidate fighter layout v0201

Measured via `vf2probe --memory-trace` from a reproducible
`native-fifth-dispatch` boundary snapshot.

## Boundary
* `build_boundary` via `decomp/i960/tools/make_game_info_probe_scenario.py`
  `build/Debug/vf2i960` + `build/Debug/vf2probe` + `roms/vf2`
  `state=8 bits=1,2,4,6,8 threshold=0`
* Snapshot: `out/state8-positive.boundary.vf2snap`
  fighters `0x00510980` / `0x00512980` (from `read_work_u32` at
  `0x00500804` / `0x00500808`), mode byte `0x0059c351`.
* `until=0x00010dcc` (scheduler return), `max_steps=1_000_000`.

## Traces (streaming to disk, not committed)
* `out/state8-case.jsonl` — `fighter0_flags=0x42` (bits 1+6), `fighter1_flags=0x02`
* `out/state8-case-bilateral.jsonl` — `0x140/0x140` (bit8+6)
* `out/state8-case-high.jsonl` — `0x200140/0x400140` (bit8+6+21 vs 26)
* `out/state4-case.jsonl` — `state 4, 0x4000/0x4000` (via `out/state4-positive.json`)

Each validated with `tools/python/trace_case.py` (internally `vf2probe --memory-trace`
plus step trace) then `tools/python/infer_structs.py --scenario --window 0x2000 --json`.

## Aggregator
`infer_structs.py` groups by `base+offset` (fighter0/fighter1),
width, and `ip_before`. Memory events are correlated by absolute
`step` (not advanced IP) per `AGENTS.md:48`.

## Stable offsets (base_count==2 in all 4 traces)

| offset | width | R/W (state8) | R/W (state4) | ips (state8) | note |
|--------|-------|--------------|--------------|--------------|------|
| +0x0000 | 4 | 6R | 6R | 0x189b0,0x18a04,0x18a08 | fighter header |
| +0x019f | 1 | 2R | 2R | 0x18a28 | |
| +0x01a4 | 4 | 4R | 4R | 0x18648,0x1864c | fighter state/flags (documented) |
| +0x01f4 | 4 | 8R | 8R | 0x1865c,0x18664,0x186a0,0x186a8 | |
| +0x01fc | 4 | 8R | 8R | 0x1866c,0x18674,0x18690,0x18698 | |
| +0x05b4 | 2 | 6R | 4R | 0x18738,0x18750,0x18788 | +0x5b6 is adjacent bit test |
| +0x05b8 | 4 | 4R+2W | 0R+2W | 0x189c4,0x189c8,0x18a24 | RW accumulation |
| +0x05f4 | 4 | 1R+2W | 2R+2W | 0x18680,0x189f8 | |
| +0x0844 | 4 | 4R | 4R | 0x1897c,0x1899c | |
| +0x1200 | 1 | 2W | 2W | 0x164d4,0x164e0 | fa_game_info zero |

Unmatched accesses: 39/88 (state8), 42/86 (state4) — outside window or
non-fighter bases (e.g. global timer, work RAM). Total field counts:
10 fields with base_count==2 in state8, 10 in state4, intersection 10.

State8-only high bits (not yet cross-state stable, for reference):
* `fighter+0x0b24` 2B RW bit15 accumulation for masks 0x00210000/0x00218000
  (see `game_info_1645c_full_state8_bit16_compounds_v0126.md`).

## Next step for taint/Z3 (AGENTS.md:225)
Desired taint output for `0x00018698`:
```
branch 0x00018698 depends on:
  fighter0 + 0x1a4 bit 6
  fighter0 + 0x5b6   (contained in 0x05b4 2B window)
```
Metadata is sideband (`model2a_observer` style), never mutates
architectural CPU state. Use 32-bit bitvectors for i960 integer ops.

## Artifact
`include/vf2/fighter_candidate.h` — provisional `struct vf2_fighter_candidate`
with `field_XXXX` neutral names, `_Static_assert` for each stable offset,
window `0x2000`. Renaming to semantic names (health, anim, etc.) requires
independent behavioral proof; this file is navigation only until then.
Do not commit `.vf2snap` or large JSONL traces.
