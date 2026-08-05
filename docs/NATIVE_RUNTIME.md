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

## Accepted fourth-dispatch corridor

The v0.1.2 runtime composes:

1. the complete observed second scheduler sweep;
2. its geometry, texture, game, tile, timer and interrupt/frame-wait path;
3. the third scheduler entry;
4. the complete observed third scheduler sweep;
5. the next geometry, input, game-state, diagnostic and frame-dispatch paths;
6. another four-visit frame wait and vector-12 interrupt; and
7. the fourth `fa_game_info` entry at `0x0001645c`.

The strict command is:

```sh
vf2i960 native-fourth-dispatch /path/to/vf2
```

It requires exactly:

- **78** repeated-frame differential blocks;
- **58,869** reference and recovered-native instructions;
- **1,329,691** continuous recovered instructions including the historical
  bridge; and
- zero native-side interpreter fallbacks.

`native-third-dispatch` remains a separate regression contract at 42 blocks and
55,239 instructions.

## Newly accepted variants

The fourth-dispatch extension recovers evidence-bounded branches for:

- the player-update gate when runtime bit 14 is set;
- non-zero game input/state selector masks;
- memory-diagnostic frame modes 16 and 17;
- the frame-counter active-selector return; and
- frame dispatcher selector 17 with non-zero phase state, bit-7-clear phase
  index and the observed clear gameplay-mask combination.

The phase-17 path preserves the live player pointers and measured procedure,
return, stack and condition-code poststate. Deeper phase-17 variants remain
unsupported.

The interrupt player-layer composes nested CPU updates against a candidate and
commits only on success. The video layer preflights its complete observed input
state before writes so a rejected mode does not partially change video or
palette memory. ROM-independent tests cover both rejection properties.

## Accounting and validation

Per-step and cumulative reports retain entry/exit addresses, block/task kinds,
scheduler indices and registry addresses, descriptor scans, frame-wait phases,
instruction counts and procedure calls/returns. Cumulative state is updated only
after an accepted runtime step succeeds.

The reusable differential runner advances the reference i960 by the exact
reported native instruction count, mirrors deterministic host frame events and
compares complete CPU state, local frames and all mutable Model 2 memory after
every block.

The current CMake configuration exposes 8 ROM-independent and 24 ROM-backed
tests. All 32 passed against the supported ROM set, and the ROM-independent
suite passed under ASan, UBSan and LeakSanitizer.

## Next integration

The current native boundary is the fourth `fa_game_info` entry. The next target
is the complete fourth scheduler sweep and the first unsupported state reached
after it. Each new branch should retain an exact differential contract and a
synthetic state regression where practical before broader fighter, object,
match, animation, collision and input types are introduced.
