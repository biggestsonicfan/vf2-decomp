# Recovered and native execution validation

v0.0.22 extends native validation across the complete first scheduler sweep,
through a 99.84%-recovered post-frame bridge and into a second scheduler
traversal. The original executor remains an independent oracle; no CPU or
memory state is copied from it after the initial live task-entry fixture.

## Accepted first sweep

The native path composes:

1. `vf2_hybrid_first_dispatch_task_execute` for seven task bodies;
2. `vf2_hybrid_first_dispatch_scheduler_advance` for six transitions;
3. `vf2_hybrid_first_dispatch_scheduler_finish` for the final task, inactive
   descriptor 28 and return to `0x0000a014`.

| Segment | Instructions |
|---|---:|
| seven task bodies | 2,808 |
| six scheduler transitions | 1,534 |
| first-sweep finish | 281 |
| total | 4,623 |

## Near-native post-frame bridge

After matching at `0x0000a014`, both machines continue independently. The
native machine dispatches accepted bridge instruction pointers to
`vf2_hybrid_post_frame_bridge_execute`; unmatched IPs still use the bounded
i960 executor.

```c
vf2_status vf2_hybrid_post_frame_bridge_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
);
```

| Block | Entry | Live invocations | Scope |
|---|---:|---:|---|
| symbol-table build | `0x0004c3f0` | 4 | compressed-symbol decoding and ROM lookup |
| pair-table build | `0x0004c4d4` | 4 | 16+4-bit pair decoding |
| byte decoder | `0x0004c6e0` | 4 | complete rows, RLE and handler dispatch |
| tree expansion | `0x0004c928` | 4 | recursive nodes and leaf-table decoding |
| word decoder | `0x0004cc28` | 4 | complete rows, literal words and RLE |
| color conversion | `0x0004ce88` | 28 | observed no-suspend conversion path |

```text
post-frame bridge total:       1,270,822 instructions
recovered in C:                1,268,752 instructions
still interpreted:                2,070 instructions
recovered blocks:                    143
memory checkpoints:                  143
frame interrupts:                      1
persistent task contexts:             29
second task entry:             0x0001645c
second registry:               0x00515200
state copied after fixture:           none
```

The fine-grained byte and word run blocks remain available internally, but the
live validator now enters the complete decoder procedures, so the 1,752 + 1,752
inner runs no longer appear as separate bridge substitutions.

## Frame-wait state machine

`vf2_hybrid_frame_wait_observe` recognizes the observed wait addresses
`0x00000f7c` and `0x00010f98`. After four visits it raises Model 2 interrupt
bit 0 and enters i960 vector 12. Reference and native machines use independent
state objects and must make the same event decision.

```c
vf2_status vf2_hybrid_frame_wait_observe(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_frame_wait_state *state,
    vf2_hybrid_frame_wait_report *report
);
```

## First geometry-facing boundary

The first observed geometry mutation is caused by instruction `0x00002eec`:

```text
st r6, 0x00003008(g10)
```

The effective 32-bit target is `0x00803008`. Its low byte was already zero, so
the first byte that changes is `0x00803009`. This establishes a precise handoff from post-frame texture preparation to
geometry-facing board control. Recovered C now reproduces this frame-commit
sequence, including the four-entry ring and the related
`0x1008/0x2008/0x3008` registers. It still does not decode
TGP polygon commands or render polygons.

Run any complete validation command:

```bash
build/vf2i960 native-second-dispatch /path/to/vf2
build/vf2i960 compare-texture-bridge /path/to/vf2
build/vf2i960 compare-post-frame-bridge /path/to/vf2
build/vf2i960 compare-geometry-boundary /path/to/vf2
```

The remaining 2,070 instructions are explicitly retained as interpreted code.
They are concentrated in top-level texture orchestration and later
geometry-preparation helpers. This is near-native bridge execution, not yet a
fully native frame loop.

## Additional v0.0.20 accepted blocks

| Block | Entry | Live invocations | Scope |
|---|---:|---:|---|
| texture address table | `0x0004d16c` | 4 | texture-bank pointer hierarchy |
| diagnostic text copy | `0x00007fc0` | 9 | strings to 16-bit tile entries |
| tile glyph expansion | `0x0004f944` | 1 | 48 glyphs / 3,072 bytes |
| palette page upload | `0x00002de4` | 1 | 28 pages / 8,064 bytes |
| conversion loop | `0x0004cdb0` | 32 | half-size iteration and call setup |
| conversion continuation | `0x0004cdd4` | 28 | state check and next iteration |
| timer/wait update | `0x00000b6c` | 8 | timer 3 and wait flag |
| video-status latch | `0x00002ec4` | 1 | board status to Work RAM |
| geometry frame commit | `0x00002edc` | 1 | command-ring register commit |
| geometry command setup | `0x00002f5c` | 1 | observed packed command word |
| frame scratch clear | `0x0000a154` | 1 | 43 dwords |


## v0.0.21 second scheduler entry

`vf2_hybrid_second_scheduler_enter()` accepts the observed main-loop call site
at `0x0000a010`. It executes the scheduler prologue, two geometry-status helper
calls, thirteen inactive descriptor scans and the final task `callx` in C.
The block accounts for 235 instructions, four calls and two returns. Alternate
ready-state branches or a different first runnable descriptor return
`VF2_ERROR_UNSUPPORTED`.


## v0.0.22 gameplay and diagnostic helpers

| Block | Entry | Live invocations | Scope |
|---|---:|---:|---|
| inline text thunk | `0x00009444` | 1 | nested copy, inline-word scan and `balx` continuation |
| texture status line | `0x0004d2c0` | 4 | label plus indexed texture name in tile RAM |
| game-state classifier | `0x0000281c` | 3 direct / 8 nested | observed zero-classification path |
| game color/control lookup | `0x000026ec` | 8 | two selectors, ROM table and color adjustment |

These blocks replace 513 additional instructions. The classifier and lookup
accept only the mode/flag combinations observed on this bridge. Other states
return `VF2_ERROR_UNSUPPORTED` before the recovered block commits a result.

```bash
build/vf2i960 compare-game-geometry-helpers /path/to/vf2
```
