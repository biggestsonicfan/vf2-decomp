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
| Reusable native runtime dispatcher | Implemented |
| Continuous execution beyond second `fa_game_info` | Not yet recovered |
| TGP protocol and renderer | Not recovered |
| Motorola 68000 / SCSP audio | Not recovered |
| Window, input and production platform backend | Not implemented |
| Playable port | No |

## Proven scope

The current zero-interpreted result covers one exact Virtua Fighter 2 Version
2.1 execution path: from the completed first scheduler sweep, through the
post-frame texture, gameplay, video, interrupt and main-loop work, to the second
entry into `fa_game_info`.

The reference i960 executor remains active only as a differential oracle in the
ROM-backed validation command. It does not produce state on the native side for
this accepted path.

This result does not prove alternate gameplay states, subsequent frames, all
scheduler task branches, rendering, TGP behavior or audio. Unsupported paths
continue to fail explicitly rather than silently invoking the interpreter.

## Native runtime layer

`include/vf2/native_runtime.h` and `src/recovered/native_runtime.c` provide the
first reusable recovered execution layer outside the developer CLI. It can:

- execute one accepted recovered block;
- route ordinary bridge, frame-wait/interrupt and second-scheduler blocks;
- run until a requested instruction boundary with a block budget;
- retain cumulative recovered instruction/call/return accounting;
- report unsupported addresses and budget exhaustion without fallback.

The dedicated ROM-independent test raises the project total to 27 CTest targets
when the supported ROM directory is configured. The implementation has also
been exercised under AddressSanitizer and UndefinedBehaviorSanitizer.

## Next acceptance target: v0.1.0

The next target is a continuous native runtime rather than another isolated
bridge percentage:

1. make `vf2i960 native-second-dispatch` consume `vf2_native_runtime` instead of
   maintaining its own candidate router;
2. profile and recover the first unsupported boundary after the second
   `fa_game_info` entry;
3. preserve all 29 task contexts across repeated scheduler cycles;
4. execute multiple consecutive frame interrupts and frame boundaries;
5. prove a bounded multi-frame run with complete CPU and mutable-memory
   comparison against the reference executor;
6. keep interpreter fallback at zero for every newly accepted path.

See `docs/NATIVE_RUNTIME.md` for the API and integration boundary.
