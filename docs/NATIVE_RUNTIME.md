# Recovered native runtime

## Purpose

`vf2_native_runtime` is the first reusable execution layer above the individual
recovered blocks. It is deliberately independent from the ROM-backed
differential CLI and never falls back to the Intel i960 interpreter.

The runtime currently routes three accepted execution classes:

- ordinary post-frame recovered bridge blocks;
- the two recovered frame-wait/interrupt phases;
- the recovered second scheduler entry.

Unknown instruction pointers and unobserved branches return
`VF2_ERROR_UNSUPPORTED` explicitly.

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

## Accounting

Both per-step and cumulative reports retain:

- entry and exit addresses;
- recovered bridge kind;
- block count;
- frame-wait phase count;
- scheduler-entry count;
- recovered instruction count;
- recovered procedure calls and returns.

The cumulative state is modified only after a recovered block completes
successfully.

## Validation

The ROM-independent test covers initialization, a zero-length run, a complete
recovered system-memory diagnostic procedure, explicit unsupported routing and
block-budget exhaustion.

The implementation was additionally exercised with the supported 36-file VF2
2.1 ROM set against all existing differential targets. The complete suite has
27 test targets after adding `vf2_native_runtime`.

## Next integration

The next step is to move candidate selection and aggregate accounting out of
`vf2i960 native-second-dispatch` and into this API. After the differential CLI
uses the same runtime layer as future platform code, execution can be extended
past the second `fa_game_info` entry one unsupported boundary at a time.
