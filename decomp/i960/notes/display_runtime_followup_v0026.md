# Display runtime follow-up helpers (v0.0.26)

Continuing the selector-3 profile corridor beyond `display_runtime_initialize` identifies two additional bounded procedures rooted at the state block addressed by `0x0050084c`.

## `0x00031040` — `display_command_emit`

This procedure is bounded at `0x000311b8` and has one direct caller at `0x0003143c`.

It reads the display runtime object from `0x0050084c` and tests bit 1 of its flags word at offset `0x40`.

The ordinary branch emits a three-component transform packet from offsets `0x54..0x5c` to the command aperture selected by `0x005001e4`, updates that aperture index after each 32-bit write, writes marker words through `(g11)[g12]`, then calls the shared helper at `0x00007c60` using the first entry of the table rooted at `0x00070cbc`.

When flag bit 1 is set, the routine first calls `display_transform_update` at `0x000311b8`, emits the same transform triple, and conditionally appends signed values from offsets `0x70` and `0x72` before selecting a table element from `0x00070cbc` using the low five bits of `0x00500020`.

The function therefore owns command serialization of the transform state initialized by `display_transform_defaults`; it is not part of the selector-3 profile-table selection itself.

## `0x000311b8` — `display_transform_update`

This procedure is bounded at `0x000313d8` and is called directly by `display_command_emit`.

Its first stage selects one of the two active runtime objects:

```text
0x00500065 == 1 ? *(u32 *)0x00500804 : *(u32 *)0x00500808
```

It then reads the display runtime block at `0x0050084c` and branches on bits 2, 3 and 4 of the flags word at offset `0x40`.

The default target transform begins from the constants:

```text
x = 0x40c00000   // 6.0f
y = 0x40966666   // 4.7f
z = 0x41940000   // 18.5f
```

When bit 2 is clear, the x target instead comes from the selected object's triple at offset `0x20c`. Other flag combinations can copy the current transform triple at display-state offset `0x54` into the selected object at offset `0x18`, inspect object flag bit 23 at `+0x1a4`, set display flag bit 4, and clamp the object halfword at `+0x1aa`.

The core update computes deltas between the current transform and target values, multiplies each by `0x3ca3d70a`, and then clamps the resulting components against ranges derived from the secondary triple at display-state offset `0x60`. This establishes `0x311b8` as a transform smoothing/update routine rather than an initializer.

## Recovery consequence

The selector-3 corridor now separates cleanly into four layers:

```text
display_profile_apply (0x1fcc0)
    -> profile constants / color-table rebuild
    -> video command submit (0x4b410)
    -> display_runtime_initialize (0x2eab8)
         -> display_transform_defaults (0x31004)
    -> video_table_expand_128 (0x11704)

later display runtime use:
    display_command_emit (0x31040)
         -> display_transform_update (0x311b8)
```

`0x31040` and `0x311b8` are not required to reproduce the immediate selector-3 initialization poststate, but they explain how the initialized transform state is consumed on later frames. They should therefore be recovered as separate procedures rather than folded into `display_runtime_initialize` or the selector-3 bridge.
