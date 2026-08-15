# Selector-3 display-profile composition (v0.0.26)

The selector-3 final-cluster bridge no longer emulates display setup through the hard-coded `selector3_apply_mode3_profile()` poststate.

It now enters and executes the recovered `display_profile_apply` procedure at `0x0001fcc0`, including its recovered children:

```text
0x0001ff0c  display_profile_mode_constants
0x0001fee4  display_profile_unit_fill
0x0001fffc  display_color_profile_apply
0x00002c38  color_table_rebuild
0x0004b410  video_command_submit
0x0002eab8  display_runtime_initialize
0x00031004  display_transform_defaults
0x00011704  video_table_expand_128
```

For the controlled profile-0 runtime vector documented in `display_profile_apply_profile0_measurement_v0026.md`, the ROM and recovered-C executions match as complete snapshots. The recovered child peels:

```text
90,373 instructions  (outer call + 90,372 recovered procedure instructions)
9 calls              (outer call + 8 nested calls)
9 returns
```

from the former synthetic aggregate:

```text
123,638 instructions
15 calls
15 returns
```

leaving, for that vector, an opaque selector-3 residual of:

```text
33,265 instructions
6 calls
6 returns
```

The residual is calculated from the recovered child report rather than hard-coded to `33,265`, so semantically supported alternate profile paths keep their own recovered accounting.

The outer bridge still restores its previously proven caller-visible CPU poststate after the recovered child, because the remaining final-cluster tail is still synthetic. Memory side effects and instruction/call/return counters from `display_profile_apply` are retained. This makes the boundary explicit: display-profile memory semantics and accounting are recovered, while the remaining tail still owns final register state until it is peeled separately.
