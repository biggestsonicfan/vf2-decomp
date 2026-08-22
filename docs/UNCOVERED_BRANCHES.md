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
bit-31 path is forced. The `fa_game_info` dispatcher and its post-call tail are
recovered in C. Both observed `0x18144` invocations now recover the
118-instruction prefix through `0x18538` plus the observed `0x17b68`
`ld`/`bbc`/`ret` helper, both observed `0x18d44` floating-port paths, and the
`0x18c64`/post-call suffix through `0x18640`, and both observed `0x18644`
shared-fighter corridors returning at `0x164b0` and `0x164c4`; unobserved
branches remain explicit ROM-backed boundaries and remain unrecovered as
native C. The observed state-4/bit-15 and non-state-4 bit-15 prefixes are now native. Controlled bit-14/15/16/6 probes now cover the `0x181c0` through
`0x184ec` conditional body; the observed bit-4/6/8/14/15/16 dependent `0x18644`
flag-accumulation paths are native. The ROM-backed state-4 fixture matrix now
matches all 192 cases for flag bits 6, 14, 15 and 16 at both non-negative and
negative thresholds; the complete negative matrix is native, removing the
dispatcher fallback for this four-bit state-4 corridor. The ROM-backed state-8
fixture matrix now
matches all 96 cases for flag bits 1, 2 and 4, and all 192 cases when flag bit
8 is included as the fourth dimension, covering isolated, asymmetric and
bilateral distributions with both countdown and mode-bit-6 settings. With
high bits 21, 26, 29, 30 and 31 appended, both threshold endpoints (`-1` and
`0`) match the complete 3,072-case matrix (256 masks × 12 distributions).
The
low-result branch now applies the same `+0x5b6` update rule as the
high-result branch when `r9 > threshold`. The state-8 negative matrix is also
native and exact for all 192 combinations of bits 1, 2, 4 and 8. State-8
negative cases composed of bits 1, 2, 4, 8, 21, 26, 29, 30 and 31 are now
admitted to the native path and match the complete 3,072-case matrix
(256 masks × 12 distributions) exactly. This includes every isolated,
asymmetric and bilateral distribution, both countdown values, both mode-bit-6
values and all high-bit/low-bit combinations. At non-negative thresholds, the
same high-bit fixture family is exact across the complete matrix as well,
including masks 248..255.
State-8 bit 6 is now admitted for the negative threshold: with the same
nine-bit set, its full ten-bit matrix matches 12,288 fixtures (1,024 masks ×
12 distributions) exactly. On the positive threshold, the recovered child
matches the complete no-bit-8 submatrix for bits 1/2/4/6 (192 fixtures), plus
the eight tested masks containing bit 6, bit 8 and all five high bits,
covering every subset of low bits 1/2/4 (96 more fixtures). Positive bit-6
compositions outside the measured slices remain unproven or explicit
boundaries. The measured
positive `0x140` (state bit 8 + bit 6) and `0x142` (state bit 8 + bits 1 + 6)
fighter-state compositions are additionally exact across all three physical
distributions, both countdown values and both mode-bit-6 settings (24 more
fixtures total). The measured positive `0x144` composition (state bit 8 + bits
2 + 6) is also exact across its 12 distributions/countdown/mode cases. The
measured positive `0x150` composition (state bit 8 + bits 4 + 6) is also exact
across its 12 distributions/countdown/mode cases. The measured positive
`0x146` composition (state bit 8 + bits 1 + 2 + 6) is now exact across its
12-case matrix, and the neighboring `0x152`, `0x154` and `0x156` compositions
are exact as well. The measured positive high-bit composition `0x4140` (state
bit 8 + bits 6 + 14) is also exact across its 12-case matrix, including its
bilateral order-dependent joins. Other positive high-bit compositions remain
unproven or explicit boundaries.
The controlled probes
now cover `g0 == 0`, `g0 == 1`, `g0 == 2` and `g0 == 3`; the shared
`0x18e08`/`0x18e00` command-port helper body and a controlled low-result
`0x18644` threshold outcome are also covered. The `0x18978..0x189a4` high-state flag tail is native as well: bits 26..29 use the measured progress/limit gate and bits 30..31 are accumulated unconditionally. The following shared `0x189a8..0x189bc` CHKBIT/ALTERBIT tail is native too, including its observable condition-code result and bit-3 accumulation. The type-22 tail now covers both the progress mismatch and the coherent equal-progress `0x18bd4` call path, including the generic `0x1ab34` type-record resolver and the measured bit-2-clear `0x18b58` branch. Unobserved downstream comparisons and other conditional branches remain ROM-backed or unsupported.

The same bridge now covers the following `fa_player` task entry at
`0x00013f08`. Its observed 842-instruction bootstrap through the first nested
call at `0x00014288`, followed by the accepted 1,652-instruction `0x19ef8`
corridor through `0x0001428c` and the downstream geometry expansion through
`0x000142c0`, followed by the small setup corridor through `0x00014310`, the
observed preamble through `0x000143e4` and its state-neutral prefix through
`0x000143fc`, the observed `0x0001ab74` entry prefix through `0x0001abf4`,
the `0x00027ce0` entry prefix through `0x00027d00`, the immediate calls to
`0x00028184` and `0x00028780`, the observed prefix through `0x00028268` and
the accepted `0x00028780` geometry body and the measured
`0x00027d90`/`0x00027dcc`/`0x00027fa0 -> 0x0002901c` and
`0x00028174 -> 0x00029414` calls and the measured `0x00014400 -> 0x00017710`,
`0x00014404 -> 0x0001791c`, `0x00014408 -> 0x0004b640`,
`0x00014414 -> 0x00016504` and `0x00014418 -> 0x000180bc` chain, is native C and matches the ROM
endpoint exactly; later player branches remain explicit original-i960
continuations. A real sixth-entry snapshot therefore
advances through both fighter task records and back to the main loop while
retaining clearly marked ROM-backed boundaries.

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
modes and signed profile overrides remain unsupported. The `0x0001fcc0`
input-profile selector is now strict-differentially recovered for the baseline
path and controlled modes 6, 10, 11 and 12. Fighter-mode pairs `2/1` and `1/2`
select mode 12, flag bits 21+20 or a live mode byte exercise mode 10, control
byte 2 redirects mode 10 to mode 11, and `0x0001ff0c` reproduces the mode-10
and mode-6 float override tables after the shared 26-float fill. The subsequent
ROM profile loader also covers `profile == 4`, including its `0x10cc` timeout
write. Seven controlled states match the reference at all three native block
boundaries (21/21 comparisons) through `0x0001fe60`; the palette wrapper then
reaches nested entry `0x00002c38`. The observed palette body is recovered for its 28-row by
32-entry RGB ramp and page latch, including the return stub at `0x00020050`.
The resumed `0x0001fe64` prefix through the `0x4b410` helper and state clears
is recovered, as is the 90-instruction `0x0002eab8` initializer and nested
`0x00031004` setup. Its `0x0001fedc` call into the 66-row `0x00011704` luma
copy and trailing return are now recovered with live pointer poststate. The
frame-dispatch bridge now covers selector 2's `0x0000ab0c` reset and advances
to selector 3. Selector 3's phase-zero `0x0000ae78` mode-table worker is now
recovered for both the live fallback profile and the zero-derived alternate
profile, including its two descriptor expanders and both ROM text sources.
Selector 17's `phase_state == 0` wrapper now follows its separate `0x00055008`
control-menu dispatcher across all 14 idle entries (0-13), every neighboring
forward/reverse transition and both 0/13 wraps. The formerly opaque screens now
reuse recovered MAIN_DATA-backed decimal/hex/text renderers plus motion,
camera/material/polygon and texture state helpers; input bit 5 covers release,
held and latched behavior on every screen, including index 13's special
43-instruction held-button early exit. Ninety-eight controlled cases are strict
ROM-backed complete-live-state matches, so there are no longer missing
phase-zero menu indices or entry transitions. Selector 3's phase table is now
complete: all eighteen entries (phases 0-17) are native, including phase 16's
countdown stay/advance pair at `0x0000c414` and phase 17's phase-reset wrap at
`0x0000c448`, each strict-matched against the reference at the `0x0000a010`
boundary. Remaining input modes, other bit-7 indirect table entries, and
unmeasured branch-level control combinations inside the now-native phase-zero
screens remain unsupported. The subsequent
`0x00002de4` palette-page upload is covered for both its inactive
condition-preserving return and its active 28-page RGB upload path.

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
