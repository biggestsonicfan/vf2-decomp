# Status

## v0.0.23

| Component | State |
|---|---|
| Repository and CMake build | Complete |
| Supported ROM validation | 36/36 files |
| Texture orchestrator cluster | Profiled; recovery targeted at v0.0.24 |
| Orchestrator evidence file | `decomp/i960/notes/texture_orchestrator_v0023.md` |
| Orchestrator trace command | `vf2i960 trace-orchestrator` (read-only developer instrument) |
| Recovered bridge blocks | 143 (unchanged from v0.0.22) |
| Post-frame bridge remaining | 2,070 interpreted instructions (unchanged) |
| `0x00001f5c` geometry-prep cluster | Deferred to v0.0.24 |

This release makes zero claims about newly recovered behavior. All v0.0.22
headline totals (1,270,822 / 1,268,752 / 2,070 / 143 / 250 / 297) are
preserved verbatim via the strict-equality assertions at
`tools/vf2i960/main.c:4305-4322`.

## v0.0.22

| Component | State |
|---|---|
| Repository and CMake build | Complete |
| Supported ROM validation | 36/36 files |
| i960 reset stack | Read from `PRCB + 24` |
| Mutable snapshot format | v5, 18 regions |
| Startup and hardware initialization | Matching C |
| `fa_*` descriptor table | 29 entries recovered |
| Timer IRQ and runtime-ready transition | Matching/proven |
| Complete first scheduler sweep | 4,623 instructions in C |
| Persistent task contexts | Preserved for all 29 descriptors |
| Second scheduler re-entry | 235 instructions in C |
| Post-frame bridge total | 1,270,822 instructions |
| Post-frame bridge recovered | 1,268,752 instructions in C |
| Post-frame bridge remaining | 2,070 interpreted instructions |
| Recovered bridge blocks | 143 |
| Complete differential checkpoints | 143 |
| Inline text thunk | 1 validated |
| Texture status lines | 4 validated |
| Game-state classifier | 3 direct + 8 nested calls |
| Game color/control lookup | 8 validated |
| Frame wait | Native four-visit state machine |
| Geometry frame commit/setup | Recovered and validated |
| First geometry instruction | `0x00002eec` |
| TGP protocol and renderer | Not recovered |
| 68000 sound recovery | Not started |
| Playable port | No |

Measured evidence:

```text
first-sweep recovered instructions:             4623
post-frame bridge instructions:              1270822
recovered bridge instructions:               1268752
interpreted bridge instructions:                2070
recovered bridge blocks:                         143
intermediate memory checkpoints:                 143
inline text thunks:                                 1
texture status lines:                               4
game-state classifier blocks:                      3
game color/control lookups:                        8
recovered bridge calls/returns:                 250/297
frame-wait threshold:                              4
frame interrupts injected:                         1
persistent task contexts:                         29
first geometry instruction:               0x00002eec
first geometry write target:              0x00803008
second scheduler entry:                    0x00010d54
second task entry:                         0x0001645c
second task registry:                      0x00515200
ROM-backed/unit CTest targets:                  22/22
```

`compare-game-geometry-helpers` initializes independent reference and native
machines from the same first-task fixture. No state is copied between them
after that fixture. Every newly accepted helper is checked with complete CPU,
local-frame and mutable-memory comparison.

This proves 99.84% recovered-C execution across the post-frame bridge. It does
not make the game playable: 2,070 bridge instructions, later task branches,
the top-level texture orchestrator, TGP command protocol, rendering and audio
remain future work.
