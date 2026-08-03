# Status

## Current master

| Component | State |
|---|---|
| Repository and warning-as-error C17 build | Configured |
| Supported ROM validation | 36/36 files |
| Accepted startup/post-frame bridge | Recovered C |
| Post-frame bridge instructions | 1,270,822 / 1,270,822 recovered |
| Native-side interpreted instructions on accepted paths | 0 |
| Original bridge CPU/memory checkpoints | 190 |
| Original bridge recovered procedure calls/returns | 342 / 340 |
| Frame wait and vector-12 interrupt | Recovered and validated |
| Second scheduler entry | Recovered and validated |
| Complete observed second scheduler sweep | Recovered and validated |
| Second-sweep extension | 566 instructions, 14 blocks, 12/14 calls/returns |
| Current continuous native boundary | Main loop `0x0000a014` |
| Reusable native runtime dispatcher | Implemented |
| Reusable native/reference differential lockstep | Implemented; ROM-backed third-dispatch integration pending |
| Third scheduler sweep evidence | Scheduler selection stable across four observed sweeps |
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

This result does not yet prove another complete frame, the full third scheduler
sweep, alternate gameplay states, rendering, TGP behavior or audio. Unsupported
paths fail explicitly rather than silently invoking the interpreter.

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
`src/recovered/native_differential.c` add a reusable lockstep validator. For
each accepted native block it advances the reference i960 by exactly the
reported recovered instruction count, captures both complete CPU/memory
snapshots and requires an exact match before continuing. The native side never
falls back to interpreted execution.

The CMake configuration defines eight ROM-independent CTest targets. When the
supported ROM directory exists, it adds 22 ROM-backed targets for a total of
30. ROM-backed tests are no longer registered merely because a default ROM path
string is non-empty. GitHub Actions configuration covers GCC and Clang Release
builds plus Clang AddressSanitizer/UndefinedBehaviorSanitizer builds; completed
remote check results must still be observed before claiming the new commits are
validated.

## Next acceptance target: v0.1.0

After consolidating status, roadmaps, subsystem modularity and multi-frame tests
in **v0.0.25**, the next target is a bounded repeated-frame native runtime in
**v0.1.0**:

1. continue from main-loop address `0x0000a014` through the recovered geometry,
   texture and frame-timer blocks;
2. execute another vector-12 frame interrupt and architectural return;
3. re-enter the scheduler for a third sweep;
4. make `vf2i960 native-third-dispatch` drive the recovered side through
   `vf2_native_differential_run_until` rather than a private candidate router;
5. preserve all 29 task contexts across repeated scheduler cycles;
6. prove the repeated-frame run with complete CPU and mutable-memory comparison;
7. keep interpreter fallback at zero for every accepted block.

`vf2i960 observe-third-sweep <rom-directory>` runs the strict second-dispatch
validator, then continues the reference i960 through subsequent scheduler
sweeps while manually injecting vector-12 interrupts at the frame-wait poll
loop. Across four observed visits, the reference always reaches the scheduler
call site `0x0000a010` with the architectural preconditions validated by
`vf2_hybrid_second_scheduler_enter` (`frame_depth == 0`, `fp == 0x005ff500`,
`r1 == 0x005ff580`, `ready_flags == 0x80004400`,
`runtime_flags == 0x00008a00`, `task_count == 29`, both timers parked at
`0x000fffff`) and selects descriptor index 13 (`fa_game_info`, `0x0001645c`,
registry `0x00515200`). The scheduler scan is therefore no longer the blocker.

The `0x0000a75c` busy subpath in `frame_geometry_gate` is recovered for both
observed transitions through `0x0000a748 -> 0x0000a800`: the
`0x0050002a != 17` retry-write and the `0x005000a6 != 0` alternate return. The
unobserved deep-reset subpath at `0x0000a784` remains explicitly unsupported.

The remaining acceptance work is to expose a ROM-backed
`native-third-dispatch` command that starts from the already validated second
`fa_game_info` boundary, executes at least one native block before applying the
same-address stop condition, and uses the reusable differential runner to find
and recover the first true repeated-frame divergence.

See `docs/NATIVE_RUNTIME.md` and `docs/NATIVE_DIFFERENTIAL.md` for the APIs and
current integration boundaries.
