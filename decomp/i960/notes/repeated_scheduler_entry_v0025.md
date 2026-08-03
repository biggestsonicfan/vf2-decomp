# Repeated scheduler entry — v0.0.25 / v0.1.0 step

## Scope

The recovered `vf2_hybrid_second_scheduler_enter` (in
`src/recovered/hybrid.c`) was historically dispatched only once per native
runtime: `vf2_native_runtime_step` short-circuited every subsequent visit
to the main-loop scheduler call site `0x0000a010` (after the second sweep
had been accounted) with a distinct
`VF2_NATIVE_RUNTIME_STEP_THIRD_SCHEDULER` step kind and
`VF2_ERROR_UNSUPPORTED` status, instead of running the actual recovered
scheduler scan. The scaffolding enabled evidence-gathering without
silently mis-running a recovery that the project had not yet proven for
repeated sweeps.

That guard is now removed. The recovered scheduler scan is generic across
sweeps because it reads live scheduler state (`ready_flags`,
`runtime_flags`, `task_count`, `timer1`, `timer2`) and walks the live task
registry at `0x00510000` using each descriptor's own `stride` field, so it
does not rely on any second-sweep-specific constant.

## Reference evidence

The `vf2i960 observe-third-sweep <rom-directory>` developer command
(CTest target `vf2_third_sweep_observation`, test 29) runs the strict
v0.0.24 second-dispatch validator and then continues the reference i960
forward through subsequent scheduler sweeps while manually injecting
vector-12 interrupts at the frame-wait poll loop. Across four observed
sweep visits at `0x0000a010` the architectural preconditions and the live
task selection are identical:

| quantity | value (each sweep visit) |
|---|---|
| `cpu.ip` (before call) | `0x0000a010` |
| `cpu.local_frame_depth` | `0` |
| `cpu.registers[FP]` | `0x005ff500` |
| `cpu.registers[1]` | `0x005ff580` |
| `state[0x00500068]` `ready_flags` | `0x80004400` |
| `state[0x00508000]` `runtime_flags` | `0x00008a00` |
| `state[0x00011d94]` `task_count` | `29` |
| `state[0x00f00000+4]` `timer1 & 0x000fffff` | `0x000fffff` |
| `state[0x00f00000+8]` `timer2 & 0x000fffff` | `0x000fffff` |
| scheduler scan selected index | `13` |
| scheduler scan selected entry | `0x0001645c` (`fa_game_info`) |
| scheduler scan selected registry | `0x00515200` |

The `frame_geometry_gate` busy subpath also fires once before sweep #2 and
twice between sweep #2 / #3 / #4 (`flags=0x0ff7f7ff`, `state[0x0050002a]=17`,
`state[0x005000a6]=255`); those busy subpaths are recovered and covered by
`test_frame_geometry_gate_busy_paths`, see
`frame_geometry_gate_busy_path_v0010.md`.

## Recovery

`vf2_native_runtime_step` now dispatches
`vf2_hybrid_second_scheduler_enter` on every visit to
`0x0000a010` regardless of `state->scheduler_entries`. The recovered scan's
own preconditions reject any state that does not match the live scheduler
snapshot above, so unsupported sweeps still fail explicitly.

The `VF2_NATIVE_RUNTIME_STEP_THIRD_SCHEDULER` step kind, the
`third_scheduler_attempts` counter on `vf2_native_runtime_state`, the
`third_scheduler_attempts` field on `vf2_native_runtime_run_report`, the
run-loop accounting for the kind, the `case` in the step-name function, and
the `test_third_scheduler_attempt_is_unsupported` test are all removed. The
behaviour is replaced by
`test_repeated_scheduler_entry_dispatches_recovery`, which proves a second
hit at `0x0000a010` after `state->scheduler_entries == 1` now forwards to
the actual recovery (which rejects an unseeded scheduler via its own
preconditions) instead of short-circuiting with the removed step kind.

## Validation

`ctest --test-dir build -C Release` (MSYS2 UCRT64 GCC 15.2.0,
`-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -Werror`,
Release): **29 / 29 tests pass, 85.86 s**. Strict v0.0.24 totals on the
accepted second-dispatch path are unchanged (`342/340` calls/returns,
`1,270,822` instructions, `frame geometry gates: 0` on the accepted
corridor).

A clang 22.1.1 + AddressSanitizer + UndefinedBehaviorSanitizer build (MSVC
SDK, `-fsanitize=address,undefined -fno-omit-frame-pointer -Werror`) also
links and passes **29 / 29 tests** with no sanitizer violations, total
runtime 449.81 s. The build needs `_CRT_SECURE_NO_WARNINGS`,
`_CRT_NONSTDC_NO_WARNINGS` and `_CRT_NONSTDC_NO_DEPRECATE` (added in
`cmake/VF2Warnings.cmake` for non-MinGW Windows targets) because MSVC's UCRT
headers mark `fopen` as deprecated.

## Remaining v0.1.0 step

The runtime no longer refuses a third scheduler sweep, but a complete
native third-sweep differential run with full CPU and mutable-memory
comparison is still required to promote the v0.1.0 repeated-frame
milestone. That step needs a new differential CLI command that drives
`vf2_native_runtime_run_until` past the second-sweep epilogue and
re-enters the scheduler scan, then compares the recovered side against
the reference i960 snapshot across the full sweep boundary.
