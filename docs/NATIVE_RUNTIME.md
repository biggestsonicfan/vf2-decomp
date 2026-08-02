# Recovered native runtime

## Purpose

`vf2_native_runtime` is the first reusable execution layer above the individual
recovered blocks. It is deliberately independent from the ROM-backed
differential CLI and never falls back to the Intel i960 interpreter.

The runtime currently routes four accepted execution classes:

- ordinary post-frame recovered bridge blocks;
- recovered task bodies, currently including the observed `fa_game_info` path;
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
- recovered bridge or task kind;
- block and recovered-task counts;
- frame-wait phase count;
- scheduler-entry count;
- recovered instruction count;
- recovered procedure calls and returns.

The cumulative state is modified only after a recovered block completes
successfully.

## Current continuous boundary

The runtime can now compose the recovered second scheduler entry with the next
observed task body:

1. scheduler call at `0x0000a010`;
2. scan of thirteen inactive descriptors;
3. selection of task index 13;
4. entry into `fa_game_info` at `0x0001645c`;
5. execution of its recovered 19-instruction observed path; and
6. architectural return to the scheduler at `0x00010dcc`.

The next unsupported boundary is therefore no longer the second task entry. It
is the continuation of the second scheduler sweep at `0x00010dcc`.

## Validation

The ROM-independent test covers initialization, a zero-length run, a complete
recovered system-memory diagnostic procedure, the second `fa_game_info` task
body, explicit unsupported routing and block-budget exhaustion.

The second-task test verifies its 19 recovered instructions, one architectural
return, registry preservation, fighter-register postconditions and return to
`0x00010dcc`.

The runtime implementation was also exercised with the supported 36-file VF2
2.1 ROM set against the existing differential targets. The project has 27 CTest
targets after adding `vf2_native_runtime`, and the relevant runtime/bridge tests
were exercised under AddressSanitizer and UndefinedBehaviorSanitizer.

## Next integration

The next work item is to recover the scheduler continuation beginning at
`0x00010dcc`, determine the next runnable descriptor in the live second sweep
and route that transition through this same runtime API. Candidate selection and
aggregate accounting in `vf2i960 native-second-dispatch` should then be replaced
by `vf2_native_runtime`, ensuring that the differential harness and future
platform executable use the same recovered execution path.
