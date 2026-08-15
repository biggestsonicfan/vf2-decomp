# Selector-3 phase-1 measurement (v0.0.26)

This note records the ROM measurements used to close selector-3 phase 1 (`0x0000afe0`) inside the frame dispatcher corridor rooted at `0x0000a6c0`.

## Observed routes

Four controlled phase-1 routes were measured from the framed `0x0000a6c0` entry through the `0x0000acf8` wrapper and its `0x00001344` fast exit:

- countdown, counter remains positive: **44 instructions / 3 calls / 4 returns**;
- countdown, counter reaches zero and advances phase 1 -> 2: **47 / 3 / 4**;
- profile measurement through `0x00002584`, result below 24 and phase jumps to 6: **171 / 9 / 10**;
- profile measurement through `0x00002584`, result at least 24 and countdown continues: **173 / 9 / 10**.

The phase-1 call tree for the measured profile route adds `0x2584`, two `0x26ec` calls and three `0x281c` calls to the wrapper calls `0xacf8`, `0xafe0`, and `0x1344`.

## Semantic corrections

ROM disassembly showed that `0x26ec` uses the mode/color lookup table when profile flag bit 1 is clear and the packed `0x3325..0x3328` RGB bytes when bit 1 is set. The earlier C translation had those paths reversed.

The `0x2584` class-2 special path is likewise selected when profile flag bit 1 is set, and class 3 returns `(0, 0)` before any zero-color rejection.

The `0xacf8` wrapper writes the signed phase to `0x00500031` and `phase << 1` to `0x00500034` before calling the phase worker. On the measured states `0x1344` returns `-1`, so `0xacf8` returns early without common selector cleanup and selector 3 remains active.

The profile-measure route reproduces the observed stack spills around `0x2584`/`0x26ec`. Commit `fcf4c3c018cde107a9fbadab9185133672548c53` additionally preserves the caller frame's `r1/sp`: the measured `x/y` results belong to the nested `0x2584` frame and must not overwrite the outer local registers before architectural return.

Implementation commits `259aa858ea37a224631aa406fbe46b7dc9a2be04` and `fcf4c3c018cde107a9fbadab9185133672548c53` apply these semantics and exact route-dependent accounting. Full native-vs-ROM snapshot validation is performed separately on the CI-built analysis binary for this source state.
