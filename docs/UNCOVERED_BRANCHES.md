# Mapping of Uncovered and Unobserved Branches (v0.1.3)

This document catalogs major unobserved execution paths and unrecovered
subsystems in Virtua Fighter 2 Version 2.1. The accepted clean-room corridor now
runs through the fifth `fa_game_info` entry, but it remains one evidence-backed
sequence rather than a complete game implementation. Unsupported paths return
`VF2_ERROR_UNSUPPORTED` instead of falling back to i960 interpretation.

## 1. Scheduler and task execution

Recovered scanning uses live registry strides, skips inactive descriptors and
handles the observed changing final active task. Still uncovered:

- dynamic task creation and deletion;
- abnormal task exits and error paths;
- corrupt or structurally different task registries;
- alternate priorities, preemption and interrupt-driven scheduling orders; and
- scheduler/task combinations not reached by the accepted repeated corridor.

## 2. Gameplay (`fa_game_info` and related gates)

The accepted corridor includes active input/state selector fast paths,
the player-update bit-14 exit and their observed sequence gates. Still missing:

The reference i960 executor now covers the conditional range comparisons and
single-precision integer/real conversions encountered when the fighter-state
bit-31 path is forced. The runtime has an explicit interpreter-backed bridge
for this task through its scheduler return; the corresponding large fighter
procedures remain unrecovered as native C.

The same bridge now covers the following `fa_player` task entry at
`0x00013f08`, allowing a real sixth-entry snapshot to advance through both
fighter task records and back to the main loop. This is still execution of the
original i960 task, not native-C recovery.

- character and arena selection;
- the complete match state machine, timeout and game-over transitions;
- fighter physics, hitboxes, hurtboxes, collision, damage and combos;
- ring-out and arena-boundary handling;
- CPU opponent decision logic; and
- evidence-backed portable fighter/object structures above raw addresses.

## 3. Camera

The startup and recurring camera corridor plus the validated optional viewport
construction paths are recovered. Unobserved movement-dependent tracking,
knockdown/throw cameras, alternate presets, zoom and mode-table transitions
remain unsupported.

## 4. Texture, video and geometry bridge

The observed texture expiration, pending palette upload and first non-zero
five-level stream expansion are recovered. Video-layer rejection preflights
inputs before writes. Still uncovered:

- alternate texture records, page formats, palette arguments and cache states;
- other stream headers, dimensions, timer states and mip layouts;
- compressed-stream corruption and invalid symbol/pair indexes;
- geometry ring-register patterns outside the accepted sequence;
- polygon FIFO packet protocol, lighting/clipping fidelity and hardware
  renderer behavior beyond the bounded direct/object reference executor; and
- production rendering output.

The observed phase-17 dispatcher path is accepted when phase state is non-zero.
Controlled ROM-backed differential evidence recovers both phase-navigation
directions in `0x00058fe0`: gameplay mask `0x08001008` advances the index with
`11 -> 0` wraparound, while bit 13 (`0x00002000`) decrements it with `0 -> 11`
wraparound. Both mark the old double-indirect phase target with `0x8020` and the
new target with `0x801c`, and the forward mask has ROM-accurate priority. The
`0x04000104` reset/display path sets phase-index bit 7, clears the 48x64 tile
plane through `0x00008ef0`, and centers the phase label via
`0x00060410 -> 0x00007fc0`.

The resulting observed bit-7 entry (`0x8b`) is also recovered: `0x00059154`
selects `0x0005ef60`, whose first visit performs meter+CRC, clears the tile plane,
draws `EXIT TEST MODE` and arms counter 320. Positive countdown visits and the
terminal `counter 1 -> 0` path at `0x0005f07c` are now recovered. The terminal
clears the observed layer/game state, writes the reset diagnostic through
`0x0006116c`, and hands off non-returningly to `0x000000b0`. Warm boot stages 1
and 2 are strict-equal through `0x0000052c`, and the post-reset continuation is
now recovered through `0x000098b0`: 60,078 instructions to `0x0006dd4c`, then
15 strict initializer blocks / 1,498,968 instructions covering descriptor-stream
copies, backup-SRAM restore, palette/table construction and hardware-core setup.
The continuation now crosses the call from `0x000098b0` into `0x0004b020`,
clears its six texture state/counter words, and derives the timer threshold in
`0x0004afb4`. It now composes the already-recovered `0x00000b6c` timer/wait
helper, returns to `0x0004afdc`, captures the initial frame byte and reaches the
status-poll loop at `0x0004afe4`. That loop now injects and resumes the shared
frame interrupt, then follows the observed frame-change exit through the status
store and call to `0x00000f7c`. The early helper's odd/high-byte wait, interrupt
resumption, `0x00002ec4` video-status latch and two caller returns are now
composed through `0x0004b07c`. The following observed equal-identity path now
checks the board/four graphics-data identities and initializes ten texture
records before entering `0x0004b820`; identity failures reject transactionally.
Its four-instruction wrapper is recovered through nested entry `0x0004b9b8`.
The observed nested setup activates record zero, clears the restart words and
unwinds to `0x000098b4`; alternate record IDs, priorities, bit-4 replacement and
mismatch diagnostic branches remain uncovered. The following `0x00011704` luma
expansion is recovered through `0x000098b8`. Its early-wait continuation now
composes the shared helper and video-status latch through `0x000098bc`. The
following `0x00011744` run-length expander's first 8,192-byte geometry pass,
frame commit and early wait are recovered through `0x0001179c`; the second
8,192-byte pass now continues the live decoder through the following frame
commit, the third reaches the next commit, and the fourth and final pass reaches
its own commit. The final commit/wait unwinds the expander, and the caller's next
early wait completes through `0x000098c4`. The following `0x000117f8` geometry
table, frame commit and early wait are recovered through its return at
`0x000098c8`. The following `0x0004ad40` reset is recovered through `0x000098cc`.
The following `0x00007f7c` and `0x00007ef0` constant-table copies are recovered
through `0x000098d4`. The existing `0x00010cbc` task-registry initializer is now
composed through its return at `0x000098d8`, and the `0x00050130` graphics-buffer
initializer returns through `0x000098dc`. The `0x0004e7b4` render-state reset and
nested 216-record clear return through `0x000098e0`. The `0x00044084` game-default
initializer and its two bounded table helpers return through `0x000098e4`. The
following `0x00053750` object-table copy and sentinel setup return through
`0x000098e8`. The `0x0000a0c4` ROM-backed effect-table copy and clear return
through `0x000098ec`. The `0x000012bc` input-ring initializer and the observed
mode-zero `0x00000fa0` diagnostic I/O initializer return through `0x000098f4`.
The inline 192 KiB game-data copy beginning there reaches `0x00009920`. The
observed `0x0000245c` display-offset initializer is recovered through its return
at `0x00009924`; alternate game-state classifications and split-screen flag
paths remain unsupported. The observed `0x0001128c` accumulator and `0x000113f4`
profile defaults are recovered through `0x0000992c`, followed by the inline
gameplay-global initialization through `0x000099fc`. Alternate accumulator
modes and signed profile overrides remain unsupported. The observed zero-mode
path through `0x0001fcc0`, its 26-float defaults and ROM profile load are
recovered through `0x0001fe60`; the palette wrapper then reaches nested entry
`0x00002c38`. The observed palette body is recovered for its 28-row by
32-entry RGB ramp and page latch, including the return stub at `0x00020050`.
The resumed `0x0001fe64` prefix through the `0x4b410` helper and state clears
is recovered, as is the 90-instruction `0x0002eab8` initializer and nested
`0x00031004` setup. Its `0x0001fedc` call into the 66-row `0x00011704` luma
copy and trailing return are now recovered with live pointer poststate. The
frame-dispatch bridge now covers selector 2's `0x0000ab0c` reset and advances
to selector 3; selector 3's mode-table worker, the subsequent
`0x00002de4` hardware-command/palette return, alternate input modes, other
bit-7 indirect table entries and phase state zero remain unsupported.

## 5. Audio and platform

Only the accepted deterministic sound-task buffer behavior, the SCSP
register/sample/MIDI host boundary, and the ROM's 68000 voice-maintenance
transition and command-dispatcher boundaries are recovered. The populated
sample-table prefix of the `0x90` allocator is also covered. The Motorola
68000 command handlers other than the bounded `0xc0`/`0xe0` paths, selected
no-live-voice `0xb0` entries and that allocator prefix, live voice/DSP synthesis,
native windowing, gamepad mappings,
frame pacing and production platform integration remain unimplemented.

## 6. Transactional rejection coverage

The player interrupt composite now preserves CPU state when a nested player
branch is rejected, and video input validation occurs before writes. Other
large composite blocks have not yet been proven globally transactional for every
unsupported subpath; additional candidate-state or rollback boundaries should
be introduced as new rejection cases are observed.
