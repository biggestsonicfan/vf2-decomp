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
- TGP command protocol, transforms, clipping, projection and rasterization; and
- production rendering output.

The observed phase-17 dispatcher path is accepted when phase state is non-zero,
phase-index bit 7 is clear and gameplay mask `0x0c00110c` is clear. Phase state
zero, alternate phase-index flags and non-zero gameplay-mask subpaths remain
unsupported. The deep geometry reset path through `0x00008ef0`, `0x0006116c`
and `0x000000b0` is also still unobserved.

## 5. Audio and platform

Only the accepted deterministic sound-task buffer behavior is recovered. The
Motorola 68000 sound-command protocol, SCSP-compatible audio backend, native
windowing, gamepad mappings, frame pacing and production platform integration
remain unimplemented.

## 6. Transactional rejection coverage

The player interrupt composite now preserves CPU state when a nested player
branch is rejected, and video input validation occurs before writes. Other
large composite blocks have not yet been proven globally transactional for every
unsupported subpath; additional candidate-state or rollback boundaries should
be introduced as new rejection cases are observed.
