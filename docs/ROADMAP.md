# Roadmap

## v0.0.1 — repository bootstrap — complete

- C17/CMake project;
- exact ROM manifest and validation;
- Model 2A region reconstruction;
- reset-vector parser and string extraction.

## v0.0.2 — i960 analysis foundation — complete

- structured decoder;
- CFG/function discovery;
- xrefs and image classification;
- first boot recovery.

## v0.0.3 — data flow and indirect control flow — complete

- abstract register interpretation;
- ABI/frame heuristics;
- indirect and jump-table recovery;
- stable symbols and generated pseudocode.

## v0.0.4 — deterministic startup validation — complete

- bounded startup memory model;
- semantic i960 executor;
- traces and snapshots;
- startup stage 1 recovered and differentially validated.

## v0.0.5 — initialization and task registry — complete

- post-IAC initialization through `0x0000052c`;
- palette, tile, I/O and interrupt initialization recovered in C;
- 29-entry `fa_*` task table recovered;
- task-registry initializer recovered and validated.

## v0.0.6 — execution fidelity and runtime checkpoint — complete

- architectural local-register frames for nested procedure calls;
- correct `balx` link-register behavior;
- additional integer and bit-branch instruction semantics;
- main-data ROM and evidence-backed Model 2A MMIO regions;
- snapshot v3 with local frames and expanded machine state;
- deterministic checkpoint at `0x0004aff8` after 2,985,244 instructions.

## v0.0.7 — interrupts and runtime synchronization — complete

- architectural external-interrupt entry and type-7 returns;
- Model 2 interrupt request/enable/acknowledge semantics;
- timer IRQ vector 14 releases the runtime wait;
- idle frame IRQ vector 12 executes deterministically;
- vector-14 timer handler recovered and differentially validated in C;
- snapshot v4 and interrupt-focused validation.

## v0.0.8 — first scheduler dispatch — complete

- proved the runtime-ready transition at `0x00009ca4`;
- identified scheduler entry `0x00010d54`;
- recovered registry scanning and runnable planning in C;
- validated 29 descriptors and seven initially runnable records;
- observed the first seven real task entries.

## v0.0.9 — first task recoveries — complete

- profiled all seven initial tasks through return;
- recovered complete `fa_user` and `fa_sound` initializer entries;
- recovered evidence-bounded first-dispatch paths for `fa_game_info` and both `fa_osage` instances;
- validated five task paths from cloned live entry snapshots;
- identified `fa_camera` and `fa_kill_osage` as the remaining initial interpreted tasks.

## v0.0.10 — camera initialization and osage cleanup — complete

- split `fa_camera` into initialization and recurring continuation phases;
- recover and validate the camera initialization prefix through `0x0001d458`;
- recover the 125-entry palette conversion helper at `0x000216b8`;
- recover the observed camera state-reset branch at `0x0001f148`;
- capture all six first-dispatch camera call sites and targets;
- completely recover `fa_kill_osage` and helper `0x00065838`;
- validate all seven initial task paths at their accepted recovery boundaries.

## v0.0.11 — recurring camera prefix — complete

- recovered helper `0x000214dc` and observed branches of `0x00020558` and `0x0001fc00`;
- described the indirect camera mode table at `0x0006e2e4`;
- modeled the coprocessor scratch state used by the observed camera calculations;
- expanded differential C validation through `0x0001d660`;
- proved that the first camera dispatch does not yet write geometry RAM.

## v0.0.12 — camera post-update gate and scheduler checkpoint — complete

- recovered the observed branch beginning at `0x0001d660`;
- proved the input-bit-3 viewport block is skipped by first-dispatch flags `0x0006`;
- recovered the observed fast exit at `0x0001e524`;
- recovered the complete non-viewport task-flag path through `0x0001d984`;
- separated the unrecovered viewport construction path from the already-proven
  arithmetic scratch traffic;
- proved the stable scheduler continuation at `0x00010dcc` after all seven
  initially runnable tasks return.

## v0.0.13 — viewport construction — complete

- recovered the input-bit-3 block from `0x0001d678` through `0x0001d8e8`;
- recovered helpers `0x0001fbb4`, `0x0001eff0` and `0x0001facc`;
- classified and reproduced the 8-entry table at task offset `0x100` and the
  10-entry table at task offset `0x150`;
- validated fixed and calculated viewport paths against synthetic entry states;
- retained the real first-dispatch observation that input flags `0x0006` skip
  this optional block.

## v0.0.14 — hybrid camera execution — complete

- substituted the three accepted live camera intervals with recovered C;
- preserved architectural register postconditions through an independent ROM
  shadow at each continuation;
- compared all mutable memory at every transition;
- advanced through the camera return and the remaining initial tasks;
- proved complete CPU and memory equality at scheduler checkpoint `0x00010dcc`;
- integrated the recovered optional viewport path into the post-update gate.

## v0.0.15 — explicit camera register postconditions — complete

- recovered the changed integer registers and instruction pointer for all three
  accepted first-dispatch camera blocks;
- recovered arithmetic condition codes and measured procedure/instruction
  counter deltas;
- proved that active saved local frames remain unchanged across the intervals;
- removed all ROM-to-hybrid CPU synchronization;
- compared complete independent snapshots at every continuation and at
  scheduler checkpoint `0x00010dcc`.

## v0.0.16 — native first-dispatch task traversal — complete

- added architectural postconditions for all seven accepted first-dispatch task paths;
- replaced the final camera `ret` with an architectural recovered-C return;
- recovered all six scheduler transitions between runnable task records;
- reproduced timing/accounting, descriptor scanning, diagnostic tile output and
  exact scheduler procedure-entry state;
- replaced 2,808 task instructions plus 1,534 scheduler instructions;
- proved zero interpreted instructions from first task entry through final
  scheduler checkpoint `0x00010dcc` on the native validation machine;
- retained a separate original execution only as a differential oracle.

## v0.0.17 — persistent task contexts and second dispatch — complete

- corrected architectural reset stack initialization from `PRCB + 24`;
- added mirrored texture RAM required by the post-frame path;
- recovered the end-of-scan scheduler path from `0x00010dcc` to `0x0000a014`;
- preserved all task and hardware state without an intermediate snapshot restore;
- injected the natural frame interrupt and reached the second scheduler traversal;
- validated the second `fa_game_info` entry with complete CPU and memory equality;
- extended the first-sweep recovered total to 4,623 instructions.

## v0.0.18 — post-frame texture bridge recovery — complete

- split the 1,270,822-instruction bridge into recovered and interpreted intervals;
- recovered byte-run and word-run texture expansion loops;
- recovered the observed recursive texture-tree expander and leaf-table path;
- recovered the observed no-suspend color-conversion procedure;
- replaced 712,821 instructions across 3,536 live invocations;
- reduced the interpreted bridge remainder to 558,001 instructions;
- proved CPU and all mutable-memory equality at intermediate checkpoints and the
  second `fa_game_info` entry.

## v0.0.19 — remaining bridge orchestration — complete

- recovered whole byte and word texture decoders instead of only their inner runs;
- recovered symbol-table and pair-table construction from the compressed bitstream;
- reduced the interpreted bridge from 558,001 to 8,346 instructions;
- replaced the frame-wait counter with a native four-visit event state machine;
- identified the first geometry-facing instruction and target address;
- preserved complete CPU and mutable-memory equality at the second task entry.

## v0.0.20 — geometry command boundary and bridge helpers — complete

- recovered the address-table, diagnostic text and tile-glyph helpers around the texture pipeline;
- recovered the complete observed palette-page upload;
- recovered texture-conversion loop orchestration and timer/wait updates;
- recovered video status, frame scratch and the first geometry commit/setup procedures;
- decoded the ring-pointer behavior of geometry registers `0x1008`, `0x2008` and `0x3008`;
- reduced the interpreted post-frame bridge from 8,346 to 2,818 instructions;
- reached 1,268,004 recovered instructions across 135 checked blocks.

## v0.0.21 — second scheduler re-entry — complete

- recovered the main-loop scheduler call at `0x0000a010`;
- reproduced the two scheduler-entry hardware helper calls;
- scanned thirteen inactive descriptors and selected task index 13;
- reconstructed the scheduler local frame and entered `fa_game_info` in C;
- replaced 235 additional bridge instructions;
- reduced the interpreted bridge remainder from 2,818 to 2,583 instructions;
- added a dedicated differential command and twenty-first test target.

## v0.0.22 — gameplay/diagnostic helpers at the geometry boundary — complete

- recovered the inline diagnostic thunk at `0x00009444`;
- recovered four texture-status lines at `0x0004d2c0`;
- recovered the observed state classifier at `0x0000281c`;
- recovered eight color/control lookups at `0x000026ec`;
- reduced the interpreted bridge from 2,583 to 2,070 instructions;
- reached 1,268,752 recovered instructions across 143 checked blocks;
- kept alternate gameplay states explicitly unsupported.

## v0.0.23 — texture orchestrator profile — complete

- added the read-only `trace-orchestrator` developer command and recorded observations of the `[0x0004bb18, 0x0004c180]` cluster;
- preserved all v0.0.22 headline totals while collecting evidence;
- backfilled `symbols.csv` and `functions.csv` with the v0.0.22 helpers;
- prepared the final texture-orchestrator and frame recovery.

## v0.0.24 — zero-interpreted second dispatch — complete

- recovered the top-level texture orchestrator and remaining geometry/gameplay/frame helpers;
- recovered the four-visit frame wait, vector-12 interrupt entry and architectural return;
- reproduced all 1,270,822 bridge instructions in C with zero native-side interpreter fallback;
- validated 190 complete CPU and mutable-memory checkpoints;
- reached 342/340 recovered procedure calls/returns;
- added a reusable native runtime dispatcher;
- recovered the entire observed second scheduler sweep from the second
  `fa_game_info` entry through recurring camera, user, sound, kill-osage and both
  osage tasks;
- added 566 recovered instructions across 14 blocks and returned to the main
  loop at `0x0000a014`.

## v0.0.25 — consolidation — complete

- updated project status, version, and roadmap to fix document inconsistencies;
- mapped all known unobserved/uncovered branches of the scheduler and tasks to `docs/UNCOVERED_BRANCHES.md`;
- refactored the post-frame bridge in `src/recovered/texture_bridge.c` into modular subsystems (`texture`, `video`, `geometry`, `input`, `match`);
- added a multi-frame execution and interrupt test to `tests/recovered/test_native_runtime.c`.

## v0.1.0 — repeated-frame native runtime — complete

- routed main-loop, geometry, texture, frame timer and interrupt blocks through
  `vf2_native_runtime`;
- executed another complete frame boundary and vector-12 interrupt;
- reached and differentially validated the third scheduler sweep entry;
- preserved all 29 task contexts across repeated cycles;
- made the differential CLI and platform-facing runtime consume the same
  dispatcher;
- retained zero native-side interpreter fallback;
- accepted 42 repeated-frame differential blocks and 55,239 instructions;
- reached 1,326,061 continuous recovered instructions.

## v0.1.1 — repeated-corridor hardening — complete

- handled dynamic late-sweep scheduling when the final active task changes;
- preserved recurring `fa_kill_osage` instruction accounting for both order-bit
  paths;
- recovered the observed expiring texture-counter transition;
- recovered the pending palette translation/upload path;
- kept unsupported texture variants explicit rather than falling back to the
  i960 interpreter;
- aligned build, public version, executable output and project documentation.

## v0.2.0 — game subsystems

- extend continuous native execution through the complete third scheduler sweep
  and subsequent repeated frames;
- fighter and object structures;
- camera, collision, animation and sound tasks;
- input and match-state logic;
- evidence-backed types for major `fa_*` states;
- replace observed-path special cases with reusable subsystem state machines as
  evidence becomes available.

## v0.3.0 — geometry path

- TGP upload extraction and analysis;
- FIFO protocol description;
- C geometry compatibility API;
- deterministic software reference renderer.

## v0.4.0 — audio path

- 68000 sound-command protocol;
- recovered sound driver logic;
- portable SCSP-compatible backend.

## v0.9.0 — first playable native build

- attract mode and one deterministic match;
- native input, rendering and audio;
- no i960 execution in the game path.

## v1.0.0 — complete native C port

- all gameplay modes;
- documented recovered game logic;
- portable renderer, audio and platform backends;
- legally owned ROMs required only for original game data;
- no general Model 2 emulator embedded in the final executable.
