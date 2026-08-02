# Status

## Current master

| Component | State |
|---|---|
| Repository and warning-as-error C17 build | Complete |
| Supported ROM validation | 36/36 files |
| Accepted startup/second-dispatch path | Recovered C |
| Post-frame bridge instructions | 1,270,822 / 1,270,822 recovered |
| Native-side interpreted instructions on that path | 0 |
| Complete CPU/memory checkpoints | 190 |
| Recovered procedure calls/returns | 342 / 340 |
| Frame wait and vector-12 interrupt | Recovered and validated |
| Second scheduler entry | Recovered and validated |
| Second `fa_game_info` observed task body | Recovered to scheduler return |
| Reusable native runtime dispatcher | Implemented |
| Current continuous native boundary | Scheduler continuation `0x00010dcc` |
| Repeated scheduler cycles and multiple frames | Not yet recovered |
| TGP protocol and renderer | Not recovered |
| Motorola 68000 / SCSP audio | Not recovered |
| Window, input and production platform backend | Not implemented |
| Playable port | No |

## Proven scope

The zero-interpreted bridge result covers one exact Virtua Fighter 2 Version
2.1 execution path: from the completed first scheduler sweep, through the
post-frame texture, gameplay, video, interrupt and main-loop work, to the second
entry into `fa_game_info`.

The reusable native runtime now continues one block beyond that original
milestone: it executes the observed 19-instruction second `fa_game_info` body and
returns architecturally to the scheduler at `0x00010dcc`.

The reference i960 executor remains active only as a differential oracle in the
ROM-backed validation commands. It does not produce state on the native side for
accepted recovered paths.

This result does not prove alternate gameplay states, subsequent scheduler
transitions, repeated frames, all task branches, rendering, TGP behavior or
audio. Unsupported paths continue to fail explicitly rather than silently
invoking the interpreter.

## Native runtime layer

`include/vf2/native_runtime.h` and `src/recovered/native_runtime.c` provide the
first reusable recovered execution layer outside the developer CLI. It can:

- execute one accepted recovered block;
- route ordinary bridge, recovered task, frame-wait/interrupt and
  second-scheduler blocks;
- compose the second scheduler entry with the observed `fa_game_info` body;
- run until a requested instruction boundary with a block budget;
- retain cumulative block, task, instruction, call and return accounting;
- report unsupported addresses and budget exhaustion without fallback.

The dedicated ROM-independent test covers the runner and the second-task
continuation. The project has 27 CTest targets when the supported ROM directory
is configured. The relevant runtime and bridge tests have also been exercised
under AddressSanitizer and UndefinedBehaviorSanitizer.

## Next acceptance target: v0.1.0

The next target is a continuous native runtime rather than another isolated
bridge percentage:

1. profile and recover the second-sweep scheduler continuation beginning at
   `0x00010dcc`;
2. identify and enter the next live runnable task through recovered C;
3. make `vf2i960 native-second-dispatch` consume `vf2_native_runtime` instead of
   maintaining its own candidate router;
4. preserve all 29 task contexts across repeated scheduler cycles;
5. execute multiple consecutive frame interrupts and frame boundaries;
6. prove a bounded multi-frame run with complete CPU and mutable-memory
   comparison against the reference executor;
7. keep interpreter fallback at zero for every newly accepted path.

See `docs/NATIVE_RUNTIME.md` for the API and current integration boundary.
