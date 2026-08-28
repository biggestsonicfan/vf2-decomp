# Natural cold scheduler frame recovery (v0136)

Fresh ROM-backed work now follows the natural cold-boot selector corridor rather than the historical selector-17 diagnostic fixture.

At the natural main-loop scheduler call site `0x0000a010`, runtime-ready bit 9 is clear. The old native runtime treated that state as a four-instruction shortcut back to `0x00009fb0`; the ROM instead enters `0x00010d54`, scans the 29-descriptor registry with the cold diagnostic-name path, and selects the first runnable task (`fa_game_info`, index 13, entry `0x0001645c`).

The recovered cold entry is deliberately fail-closed. It admits only the measured 29-task registry, clear runtime-ready bit, measured timer state, diagnostic-input mode and the observed first runnable index 13. The boundary `0x0000a010 -> 0x0001645c` is exact at 1,467 instructions, 32 calls and 30 returns.

The same cold diagnostic scheduler path is now used for the measured transitions `13 -> 17 -> 18 -> 24 -> 25 -> 26 -> 27`, covering `fa_game_info`, initial `fa_camera`, `fa_user`, `fa_sound`, `fa_kill_osage`, `fa_osage0` and `fa_osage1`. The initial camera entry `0x0001d320` was already recovered but was not reachable from the multi-block runtime dispatcher; it is now routed as a native task entry.

The cold scheduler finish after index 27 also reproduces the inactive index-28 diagnostic update, the `EXAD` marker and the stale local-frame postcondition measured from the ROM before returning to `0x0000a014`.

A one-instruction accounting correction is applied only to the measured natural frame-wait domain (`0x00010f90`, 29-task registry, runtime-ready clear). This preserves the existing generic frame-wait model while matching the cold-boot oracle exactly.

Validation against the supplied supported ROM was performed at every scheduler/task boundary through `0x0000a014`, with exact snapshot matches. The complete continuation from `0x0000a014` through the texture pipeline, frame timer, four-visit frame wait, vector-12 interrupt path, VBlank handling and selector 1 reaches the next `0x0000a010` with identical final state and counters:

- instructions: `6,872,383` reference / `6,872,383` native;
- calls: `8,787` / `8,787`;
- returns: `8,787` / `8,787`;
- final IP: `0x0000a010`;
- snapshot: exact match.

No ROM bytes or snapshots are committed.
