# Status

## Current master

| Component | State |
|---|---|
| Repository and warning-as-error C17 build | Complete |
| Supported ROM validation | 36/36 files |
| Accepted startup/post-frame bridge | Recovered C |
| Post-frame bridge instructions | 1,270,822 / 1,270,822 recovered |
| Native-side interpreted instructions on accepted paths | 0 |
| Original bridge CPU/memory checkpoints | 190 |
| Original bridge recovered procedure calls/returns | 342 / 340 |
| Frame wait and vector-12 interrupt | Recovered and validated |
| Second scheduler entry | Recovered and validated |
| Complete observed second scheduler sweep | Recovered and validated |
| Second-sweep extension | 566 instructions, 14 blocks, 12/14 calls/returns |
| Current continuous native boundary | Main loop `0x0000a014` |
| Reusable native runtime dispatcher | Implemented |
| Third scheduler sweep / repeated frame proof | Not yet recovered |
| TGP protocol and renderer | Not recovered |
| Motorola 68000 / SCSP audio | Not recovered |
| Window, input and production platform backend | Not implemented |
| Playable port | No |

## Proven scope

The zero-interpreted bridge result covers one exact Virtua Fighter 2 Version
2.1 execution path from the completed first scheduler sweep through the
post-frame texture, gameplay, video, interrupt and main-loop work to the second
entry into `fa_game_info`.

The reusable native runtime now continues through the complete observed second
scheduler sweep. It executes:

- second `fa_game_info`;
- recurring `fa_camera`;
- `fa_user`;
- recurring `fa_sound`;
- recurring `fa_kill_osage`;
- `fa_osage0` and `fa_osage1`;
- every intervening descriptor scan; and
- the scheduler epilogue returning to `0x0000a014`.

The extension adds 566 recovered instructions across 14 blocks. Complete CPU,
local-frame and mutable-memory comparison reached `MATCH` after each block.

The reference i960 executor remains active only as a differential oracle in the
ROM-backed validation commands. It does not produce state on the native side for
accepted recovered paths.

This result does not yet prove another complete frame, the third scheduler
sweep, alternate gameplay states, rendering, TGP behavior or audio. Unsupported
paths fail explicitly rather than silently invoking the interpreter.

## Native runtime layer

`include/vf2/native_runtime.h` and `src/recovered/native_runtime.c` provide the
recovered execution layer outside the developer CLI. It can:

- execute one accepted recovered block;
- route bridge, task, frame-wait/interrupt and scheduler blocks;
- scan task registries using their live stride fields;
- skip inactive descriptors and enter the next active task;
- execute the complete observed second scheduler sweep;
- run until a requested instruction boundary with a block budget;
- retain cumulative block, task, scheduler, instruction, call and return
  accounting; and
- report unsupported addresses and budget exhaustion without fallback.

Dedicated ROM-independent tests cover the runtime runner plus scheduler stride
scanning and the second-sweep epilogue. With the supported ROM directory
configured, the project now defines 28 CTest targets.

## Next acceptance target: v0.1.0

The next target is a bounded repeated-frame native runtime:

1. continue from main-loop address `0x0000a014` through the recovered geometry,
   texture and frame-timer blocks;
2. execute another vector-12 frame interrupt and architectural return;
3. re-enter the scheduler for a third sweep;
4. make the differential CLI consume `vf2_native_runtime` instead of maintaining
   a separate candidate router;
5. preserve all 29 task contexts across repeated scheduler cycles;
6. prove the repeated-frame run with complete CPU and mutable-memory comparison;
7. keep interpreter fallback at zero for every accepted block.

See `docs/NATIVE_RUNTIME.md` for the API, measured second-sweep sequence and
current integration boundary.
