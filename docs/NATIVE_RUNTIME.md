# Recovered native runtime

## Purpose

`vf2_native_runtime` is the reusable execution layer above individual recovered
blocks. It is independent from the ROM-backed differential CLI and never falls
back to the Intel i960 interpreter.

It routes accepted bridge, task, frame-wait, interrupt and scheduler states,
uses live task-registry strides, preserves persistent task contexts and reports
unknown or unobserved paths as `VF2_ERROR_UNSUPPORTED`.

## API

```c
vf2_native_runtime_state runtime;
vf2_status status = vf2_native_runtime_initialize(&runtime, 4u);

vf2_native_runtime_step_report step;
status = vf2_native_runtime_step(&machine, &cpu, &runtime, &step);

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

Repeated cycles begin and end at the same `fa_game_info` address. The native
differential layer therefore exposes a minimum-block stop contract so the
current address is not mistaken for completion of another cycle.

## Accepted fifth-dispatch corridor

The v0.1.3 runtime composes:

1. the complete observed second and third scheduler sweeps accepted previously;
2. the fourth scheduler entry;
3. the complete observed fourth scheduler sweep;
4. a large texture-orchestrator interval with a non-zero compressed stream;
5. the next geometry, game, timer, interrupt and frame-wait paths; and
6. the fifth `fa_game_info` entry at `0x0001645c`.

The strict command is:

```sh
vf2i960 native-fifth-dispatch /path/to/vf2
```

It requires exactly:

- **830** repeated-frame differential blocks;
- **7,402,741** reference and recovered-native instructions;
- **8,673,563** continuous recovered instructions including the historical
  bridge; and
- zero native-side interpreter fallbacks.

`native-third-dispatch` and `native-fourth-dispatch` remain separate regression
contracts at 42 / 55,239 and 78 / 58,869 respectively.

## Newly accepted texture interval

The fourth sweep exposes the first observed non-zero stream at `0x0004be6c`.
The recovered C path validates the header and timer preconditions, expands five
mipmap levels and reproduces the measured CPU and memory poststate:

- **35,059** instructions;
- two nested procedure calls and two returns;
- **43,648** source bytes consumed;
- **87,296** texture bytes written; and
- 5 levels with decreasing 128, 64, 32, 16 and 8 row counts.

Each 16-byte source group is copied to the destination and followed by four
32-bit values containing the upper half of each source word. Destination rows
use the observed `0x800` stride. Header variants, malformed dimensions and
unobserved timer states remain explicitly unsupported.

The same corridor also recovers a texture-record scan with nine inactive
records before the live record, the no-pending stream resume gate and the
zero-counter completion path.

## Accounting and validation

Per-step and cumulative reports retain entry/exit addresses, block/task kinds,
scheduler indices and registry addresses, descriptor scans, frame-wait phases,
instruction counts and procedure calls/returns. Cumulative state is updated only
after an accepted runtime step succeeds.

The reusable differential runner advances the reference i960 by the exact
reported native instruction count, mirrors deterministic host frame events and
compares complete CPU state, local frames and all mutable Model 2 memory after
every block.

The current CMake configuration exposes 8 ROM-independent and 25 ROM-backed
tests. All 33 pass against the supported ROM set, and the ROM-independent suite
passes under ASan, UBSan and LeakSanitizer.

## Next integration

The current native boundary is the fifth `fa_game_info` entry. The next target
is the complete fifth scheduler sweep and the first unsupported state after it,
followed by longer endurance runs. Each new branch should retain an exact
differential contract and a synthetic state regression where practical before
broader fighter, object, match, animation, collision and input types are
introduced.
