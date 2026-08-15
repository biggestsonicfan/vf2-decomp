# `display_profile_apply` profile-0 controlled measurement (v0.0.26)

This note records a ROM-executed accounting target for the already bounded `0x0001fcc0..0x0001fee0` procedure.

## Controlled starting state

The source state is the real pre-final-cluster snapshot used by the native post-frame differential, with only the instruction pointer relocated to `0x0001fcc0` so the original ROM procedure can be measured in isolation.

Relevant state before the first instruction:

```text
0x00500064 profile       = 0
0x00500068 runtime flags = 0x80004400
0x0050004c gate          = 0
0x00500804 player 0      = 0x00510980
0x00500808 player 1      = 0x00512980
player0 + 0x1b1          = 0
player1 + 0x1b1          = 0
0x00500814 display obj   = 0x00515400
0x0050084c display state = 0x00515d00
0x00078d0c expand count  = 66
```

This is deliberately **not** promoted as proof that selector-3 always reaches `0x1fcc0` with profile 0. It is a controlled differential vector taken from a realistic runtime snapshot. Its purpose is to give the recovered procedure an exact ROM accounting target without baking the old selector-3 hard-coded profile-3 assumption into the C implementation.

## Original-ROM execution

Running the original i960 code from `0x0001fcc0` and stopping at `0x0001fee0` before the final `ret` executes **90,371 instructions** and reaches all eight nested calls:

```text
0x0001fdd0 -> 0x0001ff0c  display_profile_mode_constants
0x0001ff0c -> 0x0001fee4  display_profile_unit_fill
0x0001fe60 -> 0x0001fffc  display_color_profile_apply
0x0002004c -> 0x00002c38  color_table_rebuild
0x0001fe74 -> 0x0004b410  video_command_submit
0x0001fed8 -> 0x0002eab8  display_runtime_initialize
0x0002ec1c -> 0x00031004  display_transform_defaults
0x0001fedc -> 0x00011704  video_table_expand_128
```

Including the parent `ret` at `0x0001fee0`, the complete recovered-procedure target for this vector is therefore:

```text
instructions = 90,372
nested calls = 8
returns      = 9
```

The return count is eight child returns plus the parent return. The call count intentionally excludes the caller's call instruction into `0x0001fcc0`, matching the recovered bridge-report convention.

## Why this matters for selector 3

The current selector-3 phase bridge still publishes a synthetic aggregate of:

```text
123,638 instructions
15 calls
15 returns
```

while separately emulating a small hard-coded profile-3 subset of the display setup. This controlled vector shows that the already recovered `display_profile_apply` composition can account for roughly 73% of that instruction delta by itself under a realistic profile-0 state.

The next bridge composition should therefore execute `execute_display_profile_apply()` as a recovered child, subtract its measured/reported contribution from the synthetic residual, and preserve the known outer poststate until the remaining opaque tail has been peeled. Differential validation must still decide the actual profile at the selector-3 visit; this note does not replace that observation.
