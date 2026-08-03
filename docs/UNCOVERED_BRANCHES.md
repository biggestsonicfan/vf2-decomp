# Mapping of Uncovered and Unobserved Branches (v0.0.25)

This document catalogs major unobserved execution paths, untested branches, and unrecovered subsystems across Virtua Fighter 2 (Version 2.1). The current clean-room decompilation covers only the single observed differential "boot-to-second-dispatch" startup corridor. All other paths return `VF2_ERROR_UNSUPPORTED` and remain out of scope for the current milestones.

---

## 1. Scheduler and Task Execution Layer

The scheduler scan currently skips inactive task descriptors and runs active ones on a strict, predetermined path.

### Uncovered Branches
- **Dynamic Task Creation/Deletion**: Branches where new tasks are allocated or existing tasks are unregistered dynamically (unobserved in the startup sequence).
- **Abnormal Task Exits**: Paths where tasks return error codes or unexpected results. Currently, tasks are assumed to complete cleanly.
- **Varying Task Registries**: Task descriptors with stride and size mismatches or corrupt fields.
- **Task Scheduling Priorities**: Different priority order scans or preemptive interrupts not yet modeled.

---

## 2. Gameplay Subsystems (`fa_game_info`)

The recovered game info routines in `src/game/game.c` are skeletons representing the initial startup and frame count increments.

### Uncovered Branches
- **Fighter Selection**: Different characters chosen by players (unobserved; the startup path bypasses character select).
- **Match State Machine**: Selection of different battle arenas, timeout conditions, and game-over transitions.
- **Fight Logic**: Hurtboxes, hitboxes, fighter physics, collisions, damage calculations, and combo state transitions.
- **Ring-Out / Physics Bounds**: Detection of fighters crossing the physical arena boundary.
- **AI Routines**: CPU opponent decision-making branches.

---

## 3. Camera Subsystems (`fa_camera`)

`fa_camera` is currently split into a startup initialization prefix and a recurring update.

### Uncovered Branches
- **Viewport Construction Branches**: The input-bit-3 viewport construction block is currently skipped based on startup input flags `0x0006`. Branch paths for alternate cameras, zoom-ins, or panning are completely unobserved.
- **Active Fighter Tracking**: Alternate update branches triggered by fighter movements, knockdowns, and throws.
- **Camera Presets**: The 125-entry palette conversion and mode transitions in the mode table at `0x0006e2e4`.

---

## 4. Post-Frame Hardware and Rendering Bridge

The post-frame bridge executes massive loops to decode texture bitstreams and submit geometry command buffers.

### Uncovered Branches
- **Bitstream Decoding Errors**: Compressed bitstream corruption branches, out-of-bounds symbol/pair indexing during decompression.
- **Texture Upload Pipeline**: Large/alternate texture page formats, dynamic palette uploads, and texture cache updates.
- **Geometry Registers**: Write patterns to geometry registers `0x1008`, `0x2008`, and `0x3008` besides the initial ring-pointer commits.
- **TGP microcode & protocol**: TGP commands, vector transformations, clipping, 3D projection, and rendering pipelines are represented as empty placeholders.

---

## 5. Sound and Platform Backends (`fa_sound`, `platform_null`)

Audio handling via Motorola 68000 SCSP and user interaction are not yet implemented.

### Uncovered Branches
- **Sound commands**: Any command other than the initial startup/buffer transfer is unsupported.
- **Platform Interaction**: Inputs, window resizing, Gamepad mappings, screen configurations, and frame pacing.
