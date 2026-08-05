# Status

## Current master — v0.1.1

| Component | State |
|---|---|
| Repository and warning-as-error C17 build | Validated in GCC and Clang CI |
| Supported ROM validation | 36/36 files |
| Accepted startup/post-frame bridge | Recovered C |
| Post-frame bridge instructions | 1,270,822 / 1,270,822 recovered |
| Repeated-frame corridor | 55,239 instructions across 42 differential blocks |
| Continuous recovered instructions | 1,326,061 |
| Native-side interpreted instructions on accepted paths | 0 |
| Original bridge CPU/memory checkpoints | 190 |
| Original bridge recovered procedure calls/returns | 342 / 340 |
| Frame wait and vector-12 interrupt | Recovered and validated through third dispatch |
| Second scheduler entry and complete second sweep | Recovered and validated |
| Third scheduler entry | Recovered and ROM-validated |
| Dynamic late-sweep scheduler finish | Recovered and unit-tested |
| Expiring texture counter and pending palette upload | Recovered for the observed path |
| Current ROM-proven continuous native boundary | Third `fa_game_info` at `0x0001645c` |
| Reusable native runtime dispatcher | Implemented |
| Reusable native/reference differential lockstep | Implemented, including same-address stops and frame-event mirroring |
| `vf2i960 native-third-dispatch` | ROM-backed `MATCH`: 42 blocks / 55,239 instructions |
| Repeated-frame phase-16 handler | Recovered, including tile clears, pattern replication, event queue and diagnostic text |
| TGP protocol and renderer | Not recovered |
| Motorola 68000 / SCSP audio | Not recovered |
| Window, input and production platform backend | Not implemented |
| Playable port | No |

## Proven scope

The accepted native path runs continuously from the completed first scheduler
sweep through the original post-frame bridge, the complete observed second
scheduler sweep, another frame boundary and the third scheduler entry. The
native side reaches the third `fa_game_info` task at `0x0001645c` without
executing i960 instructions.

The original startup/post-frame bridge contributes 1,270,822 recovered
instructions. The validated repeated-frame corridor contributes 55,239
instructions across 42 native differential blocks, for a continuous recovered
total of 1,326,061 instructions.

The repeated corridor includes:

- the complete observed second scheduler sweep and all 29 persistent task
  contexts;
- the repeated texture final-status zero-counter path;
- persistent video callback composition;
- the repeated game-state meter-only path;
- the tile-controller no-update path;
- vector-12 frame interrupt handling and architectural return;
- the phase-16 frame dispatcher at `0x00010a0c`, including event-queue writes,
  two 64 x 48 tile clears, tile-pattern replication, palette setup and
  diagnostic text copies; and
- scheduler re-entry selecting descriptor 13 (`fa_game_info`).

The v0.1.1 hardening increment additionally covers the observed dynamic
late-sweep scheduler finish, both recurring `fa_kill_osage` order-bit accounting
profiles, an expiring texture-counter transition and its pending palette upload.
These paths remain evidence-bounded: unknown texture variants and unobserved
state combinations still fail explicitly.

The supported 36-file ROM set was executed with:

```sh
vf2i960 native-third-dispatch /path/to/vf2
```

The command reached `MATCH` after 42 compared blocks and 55,239 instructions on
both the reference and native sides. Complete CPU state, local frames,
execution counters, frame-event state and all mutable Model 2 memory matched at
every accepted block and at the third task entry.

The reference i960 executor remains active only as a differential oracle in
ROM-backed validation commands. It does not produce state on the native side.
Unsupported or unobserved paths fail explicitly rather than silently falling
back to interpretation.

This result proves one evidence-backed repeated-frame corridor and third
scheduler entry. It does not yet prove alternate gameplay states, a complete
third scheduler sweep, TGP rendering, audio or a playable platform frontend.

## Native runtime and differential layers

`include/vf2/native_runtime.h` and `src/recovered/native_runtime.c` provide the
recovered execution layer outside the developer CLI. It can:

- execute one accepted recovered block;
- route bridge, task, frame-wait/interrupt and scheduler blocks;
- scan task registries using their live stride fields;
- skip inactive descriptors and enter the next active task;
- finish a sweep even when the final active task changes;
- execute the complete observed second scheduler sweep;
- cross the proven repeated-frame phase and enter the third scheduler cycle;
- run until a requested instruction boundary with a block budget;
- retain cumulative block, task, scheduler, instruction, call and return
  accounting; and
- report unsupported addresses and budget exhaustion without fallback.

`include/vf2/native_differential.h` and
`src/recovered/native_differential.c` provide the reusable lockstep validator.
For each accepted native block it advances the reference i960 by exactly the
reported recovered instruction count, mirrors deterministic frame-wait and
vector-12 host events, captures complete CPU/memory snapshots and requires an
exact match before continuing.

`vf2_native_differential_run_until_after` adds a minimum-block condition to the
normal stop-address contract. It allows repeated cycles that begin and end at
the same `fa_game_info` entry while preserving the original zero-length behavior
of `vf2_native_differential_run_until`.

The CMake configuration defines eight ROM-independent CTest targets. When the
supported ROM directory exists, it adds 23 ROM-backed targets for a total of
31, including `vf2_native_third_dispatch`. GitHub Actions validates GCC and
Clang Release builds plus Clang AddressSanitizer, UndefinedBehaviorSanitizer and
LeakSanitizer builds. The proprietary ROM set is not stored in the repository or
GitHub Actions; ROM-backed acceptance is executed locally against the supported
set.

## Roadmap position

v0.1.0 completed repeated-frame native-runtime acceptance, and v0.1.1 hardened
the first dynamic scheduler and texture-expiration cases discovered immediately
after that acceptance. The active roadmap stage is now v0.2.0 game subsystems.

The immediate execution target is the complete third scheduler sweep followed
by another frame boundary. Each newly observed unsupported branch should be
recovered behind the same native runtime API and accepted through differential
CPU and mutable-memory comparison before broader fighter, match, input,
animation and collision types are introduced.

Rendering, TGP protocol, audio and platform work remain later roadmap stages.

See `docs/ROADMAP.md`, `docs/NATIVE_RUNTIME.md` and
`docs/NATIVE_DIFFERENTIAL.md` for the roadmap and execution APIs.
