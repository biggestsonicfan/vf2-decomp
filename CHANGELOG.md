# Changelog

## 0.0.24 — 2026-08-02

- completed recovery of the accepted post-scheduler second-dispatch path: all
  1,270,822 original bridge instructions now execute as recovered C, with zero
  native-side interpreter fallbacks;
- composed the gameplay input/state/meter, tile controller, interrupt support,
  video, texture-orchestrator and main-loop tails from the previously recovered
  helpers without duplicating their semantics;
- replaced the final ten polling/return instructions with an explicit recovered
  frame-wait executor that preserves four observed visits, vector-12 interrupt
  injection, the i960 interrupt frame, return state and changed-frame-byte exit;
- retained step-by-step execution of the reference interpreter in the ROM-backed
  validator and compared complete CPU and mutable Model 2 memory after all 190
  recovered blocks;
- reached strict totals of 1,270,822 recovered, 0 interpreted, 190 blocks and
  memory checkpoints, and 342/340 recovered procedure calls/returns;
- added ROM-independent coverage for both recovered frame-wait phases and kept
  the full build warning-clean under C17 with warnings treated as errors;
- added `vf2_native_runtime`, a reusable no-fallback dispatcher for recovered
  bridge, task, frame-wait and scheduler blocks;
- recovered the complete observed second scheduler sweep through recurring
  camera, user, sound, kill-osage and both osage tasks;
- added 566 recovered instructions across 14 fully checked blocks, with 12
  recovered calls and 14 returns, ending at main-loop address `0x0000a014`;
- added dynamic registry-stride scanning, inactive-descriptor skipping, the
  second-sweep scheduler epilogue and dedicated ROM-independent tests;
- preserved the scope boundary: this proves observed VF2 2.1 runtime paths, not
  a complete playable port, and unsupported branches remain rejected.

## 0.0.23 — 2026-08-02

- pure-evidence release: no new recovered blocks, no new `vf2_hybrid_bridge_kind`, no `case` added to `vf2_hybrid_post_frame_bridge_execute`, and no change to the `bridge_candidate` IP list in the differential validator;
- added the read-only `trace-orchestrator` developer command, which reuses `command_native_dispatch` and emits a CSV row per interpreted native step in the `[0x0004bb18, 0x0004c180]` cluster;
- the command aborts (and writes no usable evidence) unless the existing strict total assertions still hold, keeping it provably non-behavior-changing relative to v0.0.22;
- recorded observations of the texture orchestrator cluster in `decomp/i960/notes/texture_orchestrator_v0023.md` and the default CSV path `decomp/i960/notes/texture_orchestrator_v0023.csv`;
- backfilled `decomp/i960/symbols.csv` and `decomp/i960/functions.csv` with the four v0.0.22 helpers (`0x00009444`, `0x0004d2c0`, `0x0000281c`, `0x000026ec`) that were missing from those tables;
- preserved all v0.0.22 headline totals: 1,270,822 bridge instructions, 1,268,752 recovered, 2,070 interpreted, 143 blocks/checkpoints, 250/297 calls/returns;
- deferred the `0x00001f5c` geometry-preparation cluster to v0.0.24 to avoid diluting the orchestrator evidence collection.

## 0.0.22 — 2026-08-01

- recovered the inline diagnostic thunk at `0x00009444`, including its nested text-copy call, inline-data scan, destination-row advance and architectural `balx` continuation;
- recovered four live calls to the texture-status line procedure at `0x0004d2c0`, including the `TEX`/`t4e` label, indexed texture name and tilemap destinations;
- recovered the observed game-state classifier at `0x0000281c` for three direct calls and eight nested calls;
- recovered eight live game color/control lookups at `0x000026ec`, including selector-dependent table lookup, stack preservation and the `0x00010101` adjustment;
- restricted gameplay helpers to the states actually observed by the startup bridge, returning `VF2_ERROR_UNSUPPORTED` for unproved modes and flag combinations;
- reduced the interpreted post-frame bridge from 2,583 to 2,070 instructions;
- reached 1,268,752 recovered bridge instructions across 143 checked blocks;
- kept complete CPU and mutable-memory equality at every new block and at the second `fa_game_info` entry.
