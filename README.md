# vf2-decomp

Clean-room, non-matching C17 decompilation research project for **Virtua Fighter
2 Version 2.1** on Sega Model 2A.

This repository does not contain ROMs and is not yet a playable port. It
contains ROM validation and reconstruction tools, a structured Intel i960
analyzer, a bounded semantic executor used for differential validation, and the
first game/runtime functions recovered in portable C.

## v0.0.25 milestone

The project is currently in the consolidation phase (**v0.0.25**), focusing on:
- **Consolidating project status and roadmap**, resolving document inconsistencies;
- **Mapping known unobserved/uncovered branches** of the scheduler and task routines into `docs/UNCOVERED_BRANCHES.md` to guide future differential validation;
- **Refactoring the recovered post-frame bridge** into modular, named subsystem files (texture, video, geometry, input, match), keeping the codebase highly maintainable;
- **Implementing robust multi-frame runtime unit tests** that simulate frame waiting and external interrupt flows across multiple frames.

## v0.0.24 milestone

The accepted startup path from the end of the first scheduler sweep through the
second entry into `fa_game_info` now executes entirely as recovered C. The
reference i960 interpreter is still advanced in the differential validator, but
it is no longer used to produce any CPU or Model 2 state on the native side.

The strict ROM-backed bridge totals are:

- 1,270,822 total original bridge instructions;
- 1,270,822 instructions reproduced by recovered C;
- **zero interpreted instructions** on the native path;
- 190 semantically composed blocks, each followed by a complete CPU and
  mutable-memory checkpoint;
- 342 recovered procedure calls and 340 recovered returns;
- the four-visit frame wait, vector-12 interrupt injection and interrupt return
  reproduced by the recovered frame scheduler;
- complete CPU, local-frame and all mutable-memory regions matching at the
  second `fa_game_info` entry.

The lower block count is intentional: small helpers were absorbed into complete
call-site, interrupt and main-loop procedures while retaining one implementation
of each underlying semantic helper.

This milestone does **not** mean that Virtua Fighter 2 is fully decompiled or
playable. It proves the single observed Version 2.1 startup/second-dispatch path.
Unobserved branches continue to return `VF2_ERROR_UNSUPPORTED`, and substantial
TGP, rendering, gameplay and audio work remains.

## Build

```sh
cmake -S . -B build \
  -DVF2_BUILD_TESTS=ON \
  -DVF2_WARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

To enable the ROM-backed differential tests, point CMake at a legally obtained
supported ROM directory:

```sh
cmake -S . -B build \
  -DVF2_BUILD_TESTS=ON \
  -DVF2_WARNINGS_AS_ERRORS=ON \
  -DVF2_ROM_DIR=/path/to/vf2
cmake --build build
ctest --test-dir build --output-on-failure
```

Sanitizer build:

```sh
cmake -S . -B build-san \
  -DVF2_BUILD_TESTS=ON \
  -DVF2_WARNINGS_AS_ERRORS=ON \
  -DVF2_ENABLE_SANITIZERS=ON \
  -DVF2_ROM_DIR=/path/to/vf2
cmake --build build-san
ctest --test-dir build-san --output-on-failure
```

## Main tools

Validate and reconstruct the supported ROM set:

```sh
build/vf2rom verify /path/to/vf2
build/vf2rom info /path/to/vf2
build/vf2rom extract /path/to/vf2 out/regions
```

Analyze the i960 program:

```sh
build/vf2i960 disasm /path/to/vf2 0x0001d320 64
build/vf2i960 function /path/to/vf2 0x0001d320
build/vf2i960 analyze /path/to/vf2 out/analysis
```

Exercise the recovered runtime milestones:

```sh
build/vf2i960 compare-boot /path/to/vf2
build/vf2i960 compare-init /path/to/vf2
build/vf2i960 compare-task-registry /path/to/vf2
build/vf2i960 compare-timer-irq /path/to/vf2
build/vf2i960 scheduler-dispatch /path/to/vf2
build/vf2i960 compare-first-dispatch /path/to/vf2
build/vf2i960 compare-camera-classifier /path/to/vf2
build/vf2i960 compare-camera-viewport /path/to/vf2
build/vf2i960 hybrid-first-dispatch /path/to/vf2
build/vf2i960 native-first-dispatch /path/to/vf2
build/vf2i960 native-second-dispatch /path/to/vf2
build/vf2i960 native-third-dispatch /path/to/vf2
build/vf2i960 compare-texture-bridge /path/to/vf2
build/vf2i960 compare-post-frame-bridge /path/to/vf2
build/vf2i960 compare-geometry-boundary /path/to/vf2
build/vf2i960 compare-second-scheduler-entry /path/to/vf2
build/vf2i960 compare-game-geometry-helpers /path/to/vf2
build/vf2i960 task-profile /path/to/vf2 out/first-dispatch.csv
```

## First-dispatch profile

```text
fa_game_info      19 instructions, 0 calls
fa_camera       2700 instructions, 6 calls
fa_user            1 instruction,  0 calls
fa_sound          15 instructions, 0 calls
fa_kill_osage     36 instructions, 2 calls
fa_osage0         19 instructions, 0 calls
fa_osage1         18 instructions, 0 calls
```

The task paths, scheduler transitions and camera recovery boundaries are
documented in `docs/FIRST_DISPATCH_TASKS.md`, `docs/HYBRID_EXECUTION.md`,
`decomp/i960/notes/camera_initialization.md`,
`decomp/i960/notes/camera_recurring_update.md` and
`decomp/i960/notes/camera_viewport.md` and
`decomp/i960/notes/post_frame_texture_bridge.md` and
`decomp/i960/notes/geometry_bridge_v0020.md` and
`decomp/i960/notes/second_scheduler_entry_v0021.md`.

## Repository layout

```text
config/                 exact supported ROM manifest
decomp/i960/            symbols, task descriptors and evidence notes
docs/                   architecture, status, execution and roadmap
include/vf2/            public C APIs
src/analysis/           CFG, xrefs, semantics and pseudocode generation
src/i960/               decoder, executor and snapshots
src/hardware/           bounded Model 2A memory/device model
src/recovered/          accepted semantic C recoveries
tools/vf2rom/           ROM validation and region reconstruction
tools/vf2i960/          analysis and differential-validation CLI
tests/                   ROM-independent and optional ROM-backed tests
```

## Legal scope

The repository contains no Sega game data. Users must provide their own legally
obtained ROM files. See `docs/LEGAL.md` and `THIRD_PARTY.md`.
