# v0.0.24 native second-dispatch: zero interpreted instructions

## Scope

This result covers the exact supported Virtua Fighter 2 Version 2.1 startup path
from the completed first scheduler sweep through the next entry into
`fa_game_info`. It does not claim coverage of unobserved gameplay branches or a
complete playable port.

The ROM set used for validation contains the 36 files already recognized by the
project manifest. No ROM bytes are stored in or uploaded to this repository.

## Final recovery groups

The final 205 interpreted instructions were removed in five compositional
stages:

1. complete game-input, game-state and tile-controller procedures;
2. frame-timer and interrupt save/buffer/input/restore procedures;
3. texture status, interrupt dispatch and main-loop tail clusters;
4. explicit main-loop call sites and the initial interrupt call cluster;
5. two frame-wait phases covering polling, interrupt injection, interrupt
   return and the changed-frame-byte loop exit.

Small functions already recovered earlier remain the sole implementation of
their semantics. The new wrappers call those helpers and account the original
i960 call/return instructions rather than copying their behavior.

## Frame-wait differential

The final ten instructions were not replaced with a fixed jump. The recovered
frame-wait executor:

- reads the live frame byte;
- records four visits to `0x00010f98`;
- raises Model 2 interrupt bit 0;
- enters i960 vector 12 through the real interrupt-frame implementation;
- returns architecturally from `0x00000d20`;
- records the resumed wait visit; and
- follows the observed unequal-byte exit to `0x00010fa4`.

During ROM-backed validation, the reference interpreter is still stepped once
per original instruction and the same frame-wait observer is invoked after each
reference step. CPU counters, local frames, interrupt state and all mutable
memory are compared after each recovered phase.

## Validation

The final command was:

```sh
./build/vf2i960 native-second-dispatch ./roms/vf2
```

The warning-as-error C17 build and all five ROM-independent test targets passed.
The frame-wait test additionally covers both new phases without ROM data.

The exact strict totals are:

- post-scheduler bridge instructions: **1,270,822**;
- recovered bridge instructions: **1,270,822**;
- interpreted bridge instructions: **0**;
- recovered blocks: **190**;
- complete differential memory checkpoints: **190**;
- recovered procedure calls/returns: **342 / 340**;
- frame-wait visits before injection: **4**;
- frame interrupts injected: **1**, vector **12**;
- final CPU and Model 2 memory state: **MATCH**;
- snapshot restores after first entry: **0**.

The reduction in block count is intentional: complete wrappers absorb earlier
fine-grained checkpoints while the validator still performs a full CPU and
mutable-memory comparison after every composed block.
