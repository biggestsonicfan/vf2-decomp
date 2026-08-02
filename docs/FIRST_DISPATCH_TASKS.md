# First-dispatch task recovery

## Validation boundary

v0.0.22 drives the original runtime through the natural ready transition and
validates a fully recovered-C traversal of all seven initially runnable task
paths plus all six scheduler transitions between them.

The reference and native machines start from the same live first-task snapshot.
The native machine invokes recovered task functions directly, performs each
architectural return, reproduces the scheduler scan and enters the next task
without interpreting ROM instructions. Complete CPU, local-frame, counter and
mutable-memory state is compared after every boundary. `fa_camera` has three
independently checked internal boundaries: initialization at
`0x0001d458`, the observed first recurring prefix at `0x0001d660`, and the
post-update gate ending at the observed fast return `0x0001e524`. The alternate
normal control path is recovered through `0x0001d984`, and the input-bit-3
viewport block is independently recovered through `0x0001d8e8` using fixed and
calculated synthetic differential states.

## Measured sequence

| Order | Task | Entry | Registry | Instructions | Calls | Work bytes | Copro bytes | C status |
|---:|---|---:|---:|---:|---:|---:|---:|---|
| 1 | `fa_game_info` | `0x0001645c` | `0x00515200` | 19 | 0 | 0 | 0 | observed branch recovered |
| 2 | `fa_camera` | `0x0001d320` | `0x00515400` | 2700 | 6 | 60 | 2 | init + update + post gate |
| 3 | `fa_user` | `0x00029748` | `0x00515880` | 1 | 0 | 0 | 0 | complete task recovered |
| 4 | `fa_sound` | `0x000439fc` | `0x00515d80` | 15 | 0 | 8 | 0 | first-entry initializer recovered |
| 5 | `fa_kill_osage` | `0x000657dc` | `0x00515e80` | 36 | 2 | 0 | 0 | complete task recovered |
| 6 | `fa_osage0` | `0x000640f4` | `0x00515f00` | 19 | 0 | 6 | 0 | observed initialization recovered |
| 7 | `fa_osage1` | `0x000640f4` | `0x00516180` | 18 | 0 | 6 | 0 | observed initialization recovered |

Changed-byte counts measure value differences, not store count. A task may
write a value already present without increasing the count.

## Camera call map

```text
0x0001d444 -> 0x000216b8  palette conversion, recovered
0x0001d448 -> 0x0001f148  state reset, observed branch recovered
0x0001d4d4 -> 0x0001f148  indirect mode-table call
0x0001d5cc -> 0x000214dc  camera range/state helper
0x0001d5d4 -> 0x00020558  camera derived-state helper
0x0001d650 -> 0x0001fc00  mode-specific update dispatcher
```

The initializer converts 125 indexed palette entries and installs continuation
`0x0001d458`. v0.0.22 accepts the indirect reset at `0x0001d4d4`, complete
range classifier `0x000214dc`, the observed early exits of `0x00020558` and
`0x0001fc00`, and the post-update gate beginning at `0x0001d660`. First-dispatch
flags skip the now-recovered optional viewport block and control byte `0x0050009c` selects return
`0x0001e524`. The complete non-viewport flag-update branch is recovered through
`0x0001d984`.

## Native scheduler transitions

The first-dispatch runnable indices are `13, 17, 18, 24, 25, 26, 27`. Recovered
C reproduces all six transitions, including inactive-descriptor scans:

| Transition | Descriptors scanned | Recovered instructions |
|---|---:|---:|
| 13 → 17 | 4 | 429 |
| 17 → 18 | 1 | 117 |
| 18 → 24 | 6 | 637 |
| 24 → 25 | 1 | 117 |
| 25 → 26 | 1 | 117 |
| 26 → 27 | 1 | 117 |

The transition recovery updates timing scratch and current index, renders the
next task name into the diagnostic tile field, reconstructs scheduler locals
and globals, accounts for diagnostic helper calls and enters the next task with
an architectural C `callx` equivalent. The six transitions total 1,534
instructions.

## Accepted functions

- `vf2_recovered_task_game_info_first_dispatch` handles the observed direct
  branch while fighter bit-31 flags are clear.
- `vf2_recovered_task_camera_initialize` covers entry `0x0001d320` through
  continuation `0x0001d458`, including palette helper `0x000216b8` and the
  observed branch of helper `0x0001f148`.
- `vf2_recovered_task_camera_first_update` covers `0x0001d458` through
  `0x0001d660`, including the complete scalar classifier and the observed early
  exits of the derived-state and mode-dispatch helpers.
- `vf2_recovered_task_camera_post_update_gate` covers the observed fast exit
  from `0x0001d660` to `0x0001e524` and the complete normal control-flag path
  through `0x0001d984`.
- `vf2_recovered_task_camera_viewport_construct` covers the optional block
  `0x0001d678–0x0001d8e8`, including centered ranges, fighter projection and
  both viewport lookup tables.
- `vf2_recovered_task_user_execute` is the complete one-instruction no-op task.
- `vf2_recovered_task_sound_initialize` writes continuation `0x00043abc` and
  initializes the local/global command buffers.
- `vf2_recovered_task_kill_osage_execute` is the complete task, including both
  calls to helper `0x00065838` and timer-derived age accumulation.
- `vf2_recovered_task_osage_first_dispatch` covers both instances while the
  selected fighter's secondary-simulation flag is clear.

## Commands

```sh
vf2i960 task-profile roms/vf2 out/first-dispatch.csv
vf2i960 compare-task-recoveries roms/vf2
vf2i960 compare-first-dispatch roms/vf2
vf2i960 compare-camera-classifier roms/vf2
vf2i960 compare-camera-viewport roms/vf2
vf2i960 hybrid-first-dispatch roms/vf2
vf2i960 native-first-dispatch roms/vf2
```

## Persistent second traversal

`native-second-dispatch` completes the end-of-list scheduler path in C, keeps
all 29 contexts live through the post-frame bridge and validates the second
entry of `fa_game_info` without an intermediate restore. The task bodies'
second-invocation branches are not yet claimed as recovered.
