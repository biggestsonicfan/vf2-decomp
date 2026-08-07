# vf2-decomp

Clean-room, non-matching C17 decompilation research project for **Virtua Fighter
2 Version 2.1** on Sega Model 2A.

This repository does not contain ROMs and is not yet a playable port. It
contains ROM validation and reconstruction tools, a structured Intel i960
analyzer, a bounded semantic executor used for differential validation, and the
first game/runtime functions recovered in portable C.

## v0.1.3 fifth-dispatch acceptance

The recovered native runtime now crosses the complete observed fourth scheduler
sweep and the next frame boundary. `vf2i960 native-fifth-dispatch` reaches the
fifth `fa_game_info` entry with:

- 830 repeated-frame differential blocks;
- 7,402,741 instructions on both native and reference sides;
- 8,673,563 continuous recovered instructions including the historical bridge;
- complete CPU, local-frame, execution-counter, frame-event and mutable-memory
  equality at every checkpoint; and
- **zero interpreted instructions** on the native side.

The extension recovers the observed non-zero texture stream: a 35,059-
instruction five-level mip expansion consuming 43,648 source bytes and writing
87,296 texture bytes. It also accepts leading inactive texture records before a
live record, the zero-counter texture path and the distinct mode-17 diagnostic
instruction profile.

## v0.1.2 fourth-dispatch acceptance

The recovered native runtime now crosses the complete observed third scheduler
sweep and another frame boundary. `vf2i960 native-fourth-dispatch` reaches the
fourth `fa_game_info` entry with:

- 78 repeated-frame differential blocks;
- 58,869 instructions on both native and reference sides;
- 1,329,691 continuous recovered instructions including the original bridge;
- complete CPU, local-frame, execution-counter, frame-event and mutable-memory
  equality; and
- **zero interpreted instructions** on the native side.

The extension recovers the observed player-update bit-14 fast exit, active game
input/state selector paths, frame modes 16/17 in the memory diagnostic, the
active frame-counter selector exit and the observed phase-17 dispatcher path.
Rejected player/video variants now preserve caller CPU state and avoid partial
video-memory writes for the covered rejection points.

## v0.1.1 repeated-corridor hardening

After the v0.1.0 repeated-frame acceptance, the runtime was hardened for the
first dynamic states exposed by continued execution:

- scheduler completion when the final active task changes late in a sweep;
- both observed recurring `fa_kill_osage` order-bit instruction profiles;
- expiration of the observed texture counter and creation of its pending upload;
- the observed seven-row, three-plane palette translation/upload path; and
- aligned CMake, public header, executable and documentation versioning.

Unsupported texture variants and unobserved state combinations still fail with
`VF2_ERROR_UNSUPPORTED`; the native runtime never silently falls back to the
i960 interpreter.

## v0.1.0 repeated-frame acceptance

The supported Version 2.1 ROM set validates a continuous recovered path from
the completed first scheduler sweep through the third scheduler entry.
`vf2i960 native-third-dispatch` reaches the third `fa_game_info` task with:

- 42 repeated-frame differential blocks;
- 55,239 instructions on both the native and reference sides;
- 1,326,061 continuous recovered instructions including the original
  post-frame bridge;
- complete CPU, local-frame, execution-counter and mutable-memory equality; and
- **zero interpreted instructions** on the native side.

The repeated corridor includes another frame interrupt/return, persistent task
contexts, texture/video/game/tile repeated paths and the large phase-16 frame
dispatch handler. This acceptance does **not** make the project playable: TGP
rendering, broader gameplay states, audio, input and a production platform
backend remain on the roadmap.

## v0.0.25 milestone

v0.0.25 completed the consolidation work that prepared this acceptance:

- consolidated project status and roadmap documents;
- mapped known unobserved/uncovered scheduler and task branches in
  `docs/UNCOVERED_BRANCHES.md`;
- split the recovered post-frame bridge into modular texture, video, geometry,
  input and match subsystems; and
- added multi-frame runtime tests for frame waiting and external interrupts.

## v0.0.24 milestone

The accepted startup path from the end of the first scheduler sweep through the
second entry into `fa_game_info` executes entirely as recovered C. The reference
i960 interpreter is still advanced in the differential validator, but it is no
longer used to produce any CPU or Model 2 state on the native side.

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
build/vf2i960 native-fourth-dispatch /path/to/vf2
build/vf2i960 native-fifth-dispatch /path/to/vf2
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
`decomp/i960/notes/camera_recurring_update.md`,
`decomp/i960/notes/camera_viewport.md`,
`decomp/i960/notes/post_frame_texture_bridge.md`,
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

## Snapshot endurance runner

The strict fifth-dispatch command can optionally persist its proven native
boundary as a versioned snapshot:

```sh
build/vf2i960 native-fifth-dispatch /path/to/vf2 fifth-dispatch.vf2snap
```

`vf2cycles` restores that exact CPU and mutable Model 2 state into independent
reference and recovered-native machines, then executes repeated scheduler
cycles in differential lockstep:

```sh
build/vf2cycles \
  --rom-dir /path/to/vf2 \
  --snapshot fifth-dispatch.vf2snap \
  --cycles 10 \
  --min-blocks 1 \
  --max-blocks 16384
```

The fifth-dispatch command writes both `fifth-dispatch.vf2snap` and the
versioned `fifth-dispatch.vf2snap.runtime` host-state sidecar. `vf2cycles`
loads the sidecar automatically, or accepts an explicit `--state` path.

The command stops at the first unsupported native block, reference execution
failure or state mismatch and prints the partial cycle, last recovered step and
first differing component. Add `--failure-prefix fifth-sweep-failure` to write
the last fully matched pre-block state as `.vf2snap`, `.runtime` and `.txt`
files. Re-running those files reproduces the unsupported block without replaying
the accepted corridor. Pre-block snapshots are only taken when
`--failure-prefix` is present, so normal endurance runs avoid that extra copy. A
successful run never interprets instructions on the native side.

A ROM-backed endurance run from the fifth-dispatch checkpoint has now completed
1,000 additional repeated-address cycles (36,000 blocks / 1,582,507 recovered
instructions) with exact per-block differential state equality.

For longer scouting runs, `--boundary-probe` keeps the same recovered-native and
reference instruction-count lockstep but defers the expensive complete snapshot
comparison until the repeated address closes each cycle. If a probe fails, both
machines and the runtime sidecar are restored to the exact start of that cycle,
so the same checkpoint can be replayed immediately with the strict per-block
runner. Successful probe cycles prove cycle-boundary equality only; they do not
replace the strict acceptance contract.

```sh
build/vf2cycles \
  --rom-dir /path/to/vf2 \
  --snapshot fifth-dispatch.vf2snap \
  --cycles 1000 \
  --boundary-probe \
  --failure-prefix boundary-failure \
  --output-snapshot next-boundary.vf2snap
```

`--output-snapshot` writes both the requested `.vf2snap` and its `.runtime`
sidecar after a successful run, allowing long endurance work to resume without
replaying earlier accepted cycles. Using the verified ROM set, boundary probes
have reached scheduler entry / frame IRQ 16,384 with complete cycle-end CPU,
counter, local-frame and mutable-memory equality. The stricter published
per-block corridor remains the 1,000-cycle result above.

Controlled state-transition probing has also recovered the phase-17 gameplay
bit-13 (`0x00002000`) step-back branch. From an exact pre-`main-final-cluster`
checkpoint, the reference path decrements phase index `11 -> 10`, updates the
old/new phase target markers to `0x8020`/`0x801c`, and strict differential
replay now matches the enclosing 281-instruction cluster and the complete
following 36-block cycle. This is targeted branch evidence rather than an
extension of the passive endurance claim.
