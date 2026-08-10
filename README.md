# vf2-decomp

Clean-room, non-matching C17 decompilation research project for **Virtua Fighter
2 Version 2.1** on Sega Model 2A.

This repository does not contain ROMs and is not yet a playable port. It
contains ROM validation and reconstruction tools, a structured Intel i960
analyzer, a bounded semantic executor used for differential validation, and the
first game/runtime functions recovered in portable C.

## v0.1.3 recovery head

The recovered native runtime now crosses the complete observed fourth scheduler
sweep and the next frame boundary. `vf2i960 native-fifth-dispatch` reaches the
fifth `fa_game_info` entry with:

- 830 repeated-frame differential blocks;
- 7,404,913 instructions on both native and reference sides;
- 8,675,735 continuous recovered instructions including the historical bridge;
- complete CPU, local-frame, execution-counter, frame-event and mutable-memory
  equality at every checkpoint; and
- **zero interpreted instructions** on the native side.

The extension recovers the observed non-zero texture stream: a 35,059-
instruction five-level mip expansion consuming 43,648 source bytes and writing
87,296 texture bytes. It also accepts leading inactive texture records before a
live record, the zero-counter texture path and the distinct mode-17 diagnostic
instruction profile.

## Sixth-dispatch acceptance

The same strict native differential contract now validates the next complete
repeated scheduler cycle after the fifth entry. `vf2i960 native-sixth-dispatch`
compares 866 blocks and 7,404,913 instructions, returning to `fa_game_info` at
`0x0001645c` with exact CPU and mutable-memory equality.

The current hardware boundary also includes a deterministic SCSP PCM slot
renderer, sound-board map, injected-input/framebuffer platform backend, and
TGP matrix/viewport/depth reference path with bounded geometry-stream framing.
The `vf2_game` shell now owns that graphics lifecycle through explicit frame
and geometry-submit calls.
These are compatibility layers; the original TGP packet execution, full SCSP
FM/DSP behavior and fighter/gameplay logic remain open.

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
`VF2_ERROR_UNSUPPORTED`. The native runtime has a recovered C dispatcher for
the fighter-state `fa_game_info` path, with explicit ROM-backed child-procedure
boundaries and a separate `fa_player` bridge; other unknown paths remain
explicit failures.

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
dispatch handler. This acceptance does **not** make the project playable: the
original TGP packet path, broader gameplay states, full audio synthesis and
production platform adapters remain on the roadmap.

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
build/vf2i960 native-resume /path/to/vf2 sixth-entry.vf2snap 20 0x80000000 0xa014
build/vf2i960 compare-texture-bridge /path/to/vf2
build/vf2i960 compare-post-frame-bridge /path/to/vf2
build/vf2i960 compare-geometry-boundary /path/to/vf2
build/vf2i960 compare-second-scheduler-entry /path/to/vf2
build/vf2i960 compare-game-geometry-helpers /path/to/vf2
build/vf2i960 task-profile /path/to/vf2 out/first-dispatch.csv
```

Inspect the 68000 audio program:

```sh
build/vf2m68k info /path/to/vf2
build/vf2m68k disasm /path/to/vf2 0x100 64
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

To exercise the explicit fighter-state bridges from a sixth-entry snapshot:

```sh
build/vf2i960 native-resume /path/to/vf2 sixth-entry.vf2snap 20 0x80000000 0xa014
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

A ROM-backed strict endurance chain from the fifth-dispatch checkpoint has now
completed 10,000 additional repeated-address cycles (360,000 blocks / 15,689,445
recovered instructions) with exact per-block differential state equality. The
final chained checkpoint is again `fa_game_info` at `0x0001645c`, with scheduler
entry / frame IRQ 10,003. The strict comparator now checks the two live CPU and
mutable-memory states directly, snapshot capture reuses same-sized region buffers,
and equal regions take a `memcmp` fast path while mismatches retain byte-precise
diagnostics.

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
per-block corridor is now the 10,000-cycle result above.

Controlled state-transition probing has also recovered both phase-17 navigation
directions. Gameplay bit 13 (`0x00002000`) steps backward (`11 -> 10`, with
`0 -> 11` wrap), while mask `0x08001008` steps forward (`10 -> 11`, with
`11 -> 0` wrap). Both update the old/new double-indirect phase target markers to
`0x8020`/`0x801c`; strict differential replay matches the enclosing 281/282-
instruction `main-final-cluster` variants and their complete following 36-block
cycles. This is targeted branch evidence rather than an extension of the
passive endurance claim.

The next controlled phase transition is now recovered as well. Injecting
`0x04000104` at the same pre-`main-final-cluster` boundary sets phase-index bit
7, clears the phase auxiliary byte, stores `0xff` in the companion state byte,
zeros the current object marker, clears the 48x64 tile plane to word `0x0020`,
and centers the ROM phase label (`EXIT` for index 11) through the original
`0x00060410 -> 0x00007fc0` text path. Strict differential replay matches the
12,889-instruction enclosing cluster exactly and stays equal for the following
35 blocks.

That bit-7 continuation is now recovered for the observed index-11 entry.
`0x00059154` clears the bit and dispatches through `0x0005ff00` to `0x0005ef60`.
On the first visit the recovered path reproduces the game-meter update, 15-byte
CRC, 48x64 tile clear and `EXIT TEST MODE` draw; the frame dispatcher accounts
13,286 instructions and the enclosing `main-final-cluster` matches strictly at
13,518 instructions. The path then arms a 320-frame countdown. Positive
countdown visits strictly match at 626 dispatcher / 858 cluster instructions. A
cycle-boundary probe from counter 320 validates 319 cycles (11,484 blocks /
701,481 instructions) and restores an exact checkpoint at `counter 1`. Strict
replay now continues through the terminal `counter 1 -> 0` transition at
`0x0005f07c`: the recovered path clears the observed video/gameplay state,
clears the 48x64 tile plane, emits the ROM `RESET` diagnostic through
`0x0006116c`, and performs the non-returning branch to `0x000000b0`. The
enclosing `main-final-cluster` matches at 13,426 instructions.

The native runtime also accepts that warm soft-reset handoff through the existing
boot recovery without pretending it is a power-on reset. Boot stage 1 preserves
untouched incoming registers/control state and matches 1,180,053 instructions to
`0x000001b0`; boot stage 2 matches another 182,514 instructions to `0x0000052c`.
Together with the terminal cluster, the three strict blocks cover 1,375,993
instructions with exact CPU, local-frame, procedure-counter and mutable-memory
equality. The warm-reset continuation now extends through the observed post-boot
initializer to caller boundary `0x000098b0`. The existing `0x0000052c ->
0x0006dd4c` prefix contributes 60,078 strict instructions; from `0x0006dd4c`,
15 additional recovered blocks contribute 1,498,968 instructions with exact
CPU, local-frame, procedure-counter and mutable-memory equality after every
block. Combined with the terminal and warm boot stages, the controlled
soft-reset chain is now compositionally proven for 2,935,039 instructions.

The recovered initializer includes both descriptor-driven bulk streams (4
descriptors / 464 words and 22 descriptors / 92,672 halfwords), valid backup-SRAM
probe/CRC/restore, both video-ramp passes, palette/table construction and the
observed geometry/video hardware-core setup. The second ramp is intentionally
11,245 instructions rather than the first pass's 11,563 because its restored
`0x40/0x25` controls take different clamp branches; runtime accounting derives
that count from the data rather than the call site. Recovery now crosses the call
at `0x000098b0` into the texture/graphics initializer, clears the six observed
state/counter words in `0x0004b020`, and derives the timer threshold in
`0x0004afb4`. Recovery now composes the shared timer/wait helper return, captures
the initial frame byte at `0x0004afdc`, and reproduces the asynchronous status
poll at `0x0004afe4` through interrupt injection and resumption. The frame-change
exit now completes the early wait helper at `0x00000f7c`, reuses the recovered
video-status latch and unwinds both callers to `0x0004b07c`. The following
82-instruction observed path now verifies the board and four graphics-data
identities, initializes ten texture records transactionally and enters
`0x0004b820`; its four-instruction wrapper is also recovered through the nested
record setup at `0x0004b9b8`. The observed setup initializes record zero, clears
the three texture restart words and unwinds to `0x000098b4` in 24 instructions.
The following recovered `0x00011704` expansion consumes 66 rows of 128 ROM bytes
and writes 8,448 zero-extended luma words in 50,891 instructions, reaching
`0x000098b8`. That caller now enters the shared early frame wait, composes its
video-status latch and returns to `0x000098bc`. The following `0x00011744`
run-length expander now emits the first 8,192-byte geometry pattern in 63,799
instructions, commits the geometry frame, performs the next early wait and
returns to `0x0001179c`. Recovery now continues the live decoder state through
the second 8,192-byte pass in 63,742 instructions, ending with pattern word
`0x4b4b4b4b` at the next frame commit. The third pass now continues through
another 8,192 bytes in 63,700 instructions and reaches the following frame
commit with terminal word `0x67676767`. The fourth and final pass emits the last
8,192 bytes in 63,679 instructions and reaches its frame commit with terminal
word `0x80808080`. The final commit and wait now unwind `0x00011744`, the caller's
following early wait completes through `0x000098c4`, and the observed
`0x000117f8` geometry-table initializer now clears its setup words, emits two
mode commands plus 32 command/value pairs and reaches the frame commit. That
commit and early wait now complete, and the initializer returns to `0x000098c8`.
The `0x0004ad40` reset now clears three graphics-state halfwords and the
`0x00546000` word, returning at `0x000098cc`. The call into `0x00007f7c` is the
new boundary. Other
bit-7 table entries and phase state zero remain unsupported.
