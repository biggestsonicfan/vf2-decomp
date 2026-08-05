# Recovered native runtime

## Purpose

`vf2_native_runtime` is the reusable execution layer above the individual
recovered blocks. It is independent from the ROM-backed differential CLI and
never falls back to the Intel i960 interpreter.

The runtime routes accepted execution classes for:

- ordinary post-frame recovered bridge blocks;
- recovered task bodies;
- recovered frame-wait and interrupt phases;
- repeated scheduler entries;
- generic scheduler transitions and inactive-descriptor scans; and
- scheduler completion, including a dynamically changing final active task.

Unknown instruction pointers and unobserved branches return
`VF2_ERROR_UNSUPPORTED` explicitly. Repeated hits of the main-loop scheduler
call site `0x0000a010` use the same recovered scheduler scan when architectural
preconditions match. `vf2i960 observe-third-sweep` confirmed that task selection
at the accepted third entry is identical to the second sweep.

## API

Initialize persistent frame-event and accounting state:

```c
vf2_native_runtime_state runtime;
vf2_status status = vf2_native_runtime_initialize(&runtime, 4u);
```

Execute one accepted recovered block:

```c
vf2_native_runtime_step_report step;
status = vf2_native_runtime_step(&machine, &cpu, &runtime, &step);
```

Execute recovered blocks until a named boundary:

```c
vf2_native_runtime_run_report run;
status = vf2_native_runtime_run_until(
    &machine,
    &cpu,
    &runtime,
    stop_address,
    max_blocks,
    &run
);
```

`run_until` succeeds immediately when the CPU is already at the requested stop
address. Exhausting the block budget is reported as unsupported and leaves a
complete partial report.

Repeated cycles that begin and end at the same instruction address use the
minimum-block contract exposed by the native differential layer so that a stop
at the current address is not confused with completion of another frame.

## Accounting

Both per-step and cumulative reports retain:

- entry and exit addresses;
- recovered bridge or task kind;
- current and next scheduler task indices;
- current and next registry addresses;
- number of descriptors scanned;
- block, task, frame-wait, scheduler-entry, transition and finish counts;
- recovered instruction count; and
- recovered procedure calls and returns.

The cumulative state is modified only after a recovered block completes
successfully.

## Accepted repeated-frame corridor

The runtime composes the entire observed second scheduler sweep and the next
frame boundary:

1. execute `fa_game_info` at `0x0001645c`;
2. scan task descriptors 14 through 17 and enter recurring `fa_camera` at
   `0x0001d458`;
3. execute the recurring camera update and return to `0x00010dcc`;
4. enter and execute `fa_user`;
5. skip inactive descriptors and enter recurring `fa_sound` at `0x00043abc`;
6. execute its deterministic buffer transfer and return;
7. enter recurring `fa_kill_osage`;
8. enter and execute `fa_osage0` and `fa_osage1`;
9. finish the scheduler and return to the main loop at `0x0000a014`;
10. execute the accepted geometry, texture, game-state, tile and frame-timer
    blocks;
11. cross the four-visit frame wait and vector-12 interrupt; and
12. re-enter the scheduler at the third `fa_game_info` task.

The transition executor reads each registry stride from offset `+0x08`. It does
not assume a fixed registry size and skips inactive descriptors until it reaches
the next active task. The scheduler finish path also accepts an earlier final
active descriptor when the remaining records are inactive and their live
strides lead to the end registry.

The accepted continuous interval contains:

- **42** repeated-frame differential blocks after the historical bridge;
- **55,239** repeated-frame instructions reproduced by C;
- **1,326,061** continuous recovered instructions including the original
  post-frame bridge; and
- zero native-side interpreter fallbacks.

## Task and texture variants

The recurring camera path composes the recovered update and post-update blocks
while preserving the live fighter cursor that differs from the first camera
invocation.

The recurring sound path copies the observed queued word into the global sound
state, clears the source and control fields, and returns in 20 instructions.

The recurring `fa_kill_osage` path shares the recovered memory and register
semantics while accounting for both observed instruction profiles: 33
instructions when order bit 0 skips the pointer swap, and 36 instructions when
the swap executes.

The texture runtime now recovers the observed transition when the second texture
counter expires. It creates the pending upload record and accepts the observed
palette upload variant that translates seven rows across three planes. Other
pending-record indices, arguments or texture formats remain explicitly
unsupported until observed and differentially validated.

## Validation

The ROM-backed differential run compares complete CPU state, architectural
local-register frames, execution counters, frame-event state and all mutable
Model 2 memory after every accepted block. The published third-dispatch corridor
reached `MATCH` for all 42 repeated-frame blocks.

ROM-independent tests cover:

- initialization and zero-length execution;
- complete recovered bridge procedures;
- repeated `fa_game_info` execution;
- dynamic registry-stride scanning across inactive descriptors;
- scheduler completion after an earlier final active task;
- both recurring kill-osage order-bit accounting paths;
- texture counter expiration and pending palette upload;
- unsupported routing; and
- block-budget exhaustion.

GitHub Actions builds warning-as-error Release configurations with GCC and Clang
and a Clang sanitizer configuration. ROM-backed tests require a legally obtained
supported ROM directory and are therefore executed outside public CI.

## Next integration

The current continuous native boundary is the third `fa_game_info` entry at
`0x0001645c`. The next target is to execute the complete third scheduler sweep,
then cross another frame boundary and reach the fourth scheduler entry.

Newly observed paths should be added to the same runtime dispatcher, covered by
ROM-independent state fixtures where practical, and accepted against the
reference i960 with complete CPU and mutable-memory comparison. This work is the
execution foundation for the v0.2.0 fighter, object, match, input, animation and
collision subsystem types.
