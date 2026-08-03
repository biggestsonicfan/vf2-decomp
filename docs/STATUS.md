# Status

## Current master

| Component | State |
|---|---|
| Repository and warning-as-error C17 build | Validated in GCC and Clang CI |
| Supported ROM validation | 36/36 files |
| Accepted startup/post-frame bridge | Recovered C |
| Post-frame bridge instructions | 1,270,822 / 1,270,822 recovered |
| Native-side interpreted instructions on accepted paths | 0 |
| Original bridge CPU/memory checkpoints | 190 |
| Original bridge recovered procedure calls/returns | 342 / 340 |
| Frame wait and vector-12 interrupt | Recovered and validated through second dispatch |
| Second scheduler entry | Recovered and validated |
| Complete observed second scheduler sweep | Recovered and validated |
| Second-sweep extension | 566 instructions, 14 blocks, 12/14 calls/returns |
| Current ROM-proven continuous native boundary | Main loop `0x0000a014` |
| Reusable native runtime dispatcher | Implemented |
| Reusable native/reference differential lockstep | Implemented, including same-address stops and frame-event mirroring |
| `vf2i960 native-third-dispatch` | Implemented; ROM-backed execution pending |
| Third scheduler sweep evidence | Scheduler selection stable across four observed reference sweeps |
| Repeated-frame geometry busy branch | Observed retry and alternate-return paths recovered |
| TGP protocol and renderer | Not recovered |
| Motorola 68000 / SCSP audio | Not recovered |
| Window, input and production platform backend | Not implemented |
| Playable port | No |

## Proven scope

The zero-interpreted bridge result covers one exact Virtua Fighter 2 Version
2.1 execution path from the completed first scheduler sweep through the
post-frame texture, gameplay, video, interrupt and main-loop work to the second
entry into `fa_game_info`.

The reusable native runtime continues through the complete observed second
scheduler sweep. It executes:

- second `fa_game_info`;
- recurring `fa_camera`;
- `fa_user`;
- recurring `fa_sound`;
- recurring `fa_kill_osage`;
- `fa_osage0` and `fa_osage1`;
- every intervening descriptor scan; and
- the scheduler epilogue returning to `0x0000a014`.

The extension adds 566 recovered instructions across 14 blocks. Complete CPU,
local-frame and mutable-memory comparison reached `MATCH` after each block.

The reference i960 executor remains active only as a differential oracle in the
ROM-backed validation commands. It does not produce state on the native side for
accepted recovered paths.

This result does not yet claim another complete ROM-backed frame or the full
third scheduler sweep. The command for that acceptance run now exists, but the
proprietary ROM set is intentionally absent from GitHub Actions. Alternate
gameplay states, rendering, TGP behavior and audio also remain outside the
proven scope. Unsupported paths fail explicitly rather than silently invoking
the interpreter.

## Native runtime and differential layers

`include/vf2/native_runtime.h` and `src/recovered/native_runtime.c` provide the
recovered execution layer outside the developer CLI. It can:

- execute one accepted recovered block;
- route bridge, task, frame-wait/interrupt and scheduler blocks;
- scan task registries using their live stride fields;
- skip inactive descriptors and enter the next active task;
- execute the complete observed second scheduler sweep;
- run until a requested instruction boundary with a block budget;
- retain cumulative block, task, scheduler, instruction, call and return
  accounting; and
- report unsupported addresses and budget exhaustion without fallback.

`include/vf2/native_differential.h` and
`src/recovered/native_differential.c` provide the reusable lockstep validator.
For each accepted native block it advances the reference i960 by exactly the
reported recovered instruction count, mirrors the deterministic frame-wait and
vector-12 host events on the reference side, captures complete CPU/memory
snapshots and requires an exact match before continuing. The native side never
falls back to interpreted execution.

`vf2_native_differential_run_until_after` adds a minimum-block condition to the
normal stop-address contract. It allows repeated cycles that begin and end at
the same `fa_game_info` entry while preserving the original zero-length behavior
of `vf2_native_differential_run_until`.

The CMake configuration defines eight ROM-independent CTest targets. When the
supported ROM directory exists, it adds 23 ROM-backed targets for a total of
31, including `vf2_native_third_dispatch`. ROM-backed tests are not registered
merely because a default ROM path string is non-empty. GitHub Actions validates
GCC and Clang Release builds plus Clang AddressSanitizer,
UndefinedBehaviorSanitizer and LeakSanitizer builds.

## Next acceptance target: v0.1.0

After consolidating status, roadmaps, subsystem modularity and multi-frame tests
in **v0.0.25**, the next target remains a bounded repeated-frame native runtime
in **v0.1.0**:

1. run `vf2i960 native-third-dispatch <rom-directory>` from the already
   validated second `fa_game_info` boundary;
2. execute the recovered main-loop, geometry, texture and frame-timer blocks;
3. mirror another vector-12 frame interrupt and architectural return on the
   reference and native sides;
4. re-enter the scheduler for a third sweep;
5. preserve all 29 task contexts across the repeated cycle;
6. require complete CPU, execution-counter and mutable-memory equality after
   every recovered block; and
7. keep interpreter fallback at zero for every accepted native block.

The command now uses `vf2_native_differential_run_until_after` with a minimum of
one block because both its start and destination are `fa_game_info` at
`0x0001645c`. Its strict acceptance profile expects the recovered repeated-frame
corridor to reach the third task entry after 42 compared blocks and 55,239
instructions. Those totals are encoded as rejection guards, not claimed as a
new ROM-backed result until the command is executed against the supported ROM
set.

`vf2i960 observe-third-sweep <rom-directory>` remains the evidence-gathering
fallback. Across four observed reference visits, it reaches scheduler call site
`0x0000a010` with the architectural preconditions validated by
`vf2_hybrid_second_scheduler_enter` (`frame_depth == 0`, `fp == 0x005ff500`,
`r1 == 0x005ff580`, `ready_flags == 0x80004400`,
`runtime_flags == 0x00008a00`, `task_count == 29`, both timers parked at
`0x000fffff`) and selects descriptor index 13 (`fa_game_info`, `0x0001645c`,
registry `0x00515200`). The scheduler scan is therefore no longer the blocker.

The `0x0000a75c` busy subpath in `frame_geometry_gate` is recovered for both
observed transitions through `0x0000a748 -> 0x0000a800`: the
`0x0050002a != 17` retry-write and the `0x005000a6 != 0` alternate return. The
unobserved deep-reset subpath at `0x0000a784` remains explicitly unsupported.

See `docs/NATIVE_RUNTIME.md` and `docs/NATIVE_DIFFERENTIAL.md` for the APIs and
current integration boundaries.
