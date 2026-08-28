# Warm selector and scheduler recovery (v0143)

This recovery rebases the previously staged warm-selector work onto the current native runtime instead of applying the stale v0142 patch mechanically.

Recovered boundaries and behavior:

- `0x00009f74 -> 0x00009fb0`: post-coprocessor delay, 2,100,198 instructions, 1 call, 1 return. The native path preserves the observed 700,000-iteration helper semantics without spending 2.1M host interpreter steps.
- selector-0 signature path `0x0000a6c0 -> 0x0000a010`: when the four-word signature helper returns `g0 == 0xffffffff`, selector 0 advances directly to selector 2. The recovered aggregate is 34 instructions, 2 calls, 3 returns.
- warm scheduler entry accepts the observed three-deep caller stack and the corresponding four-deep scheduler return state instead of treating those states as synthetic corruption.
- synthetic `r13` frame-depth instrumentation is restricted to depth-zero instrumentation paths and no longer overwrites live warm register state.
- `fa_object` (`0x0006ca64`) is accepted as the generic object task: the instance byte selects the entry from the ROM table at `0x0006ca78`, writes it to descriptor `+0x0c`, and returns through the scheduler.
- `fa_game_disp` (`0x0002b1bc`) is an explicit interpreted bridge while its native body remains unrecovered; the bridge preserves the observed post-condition state instead of silently approximating it.

Validation performed against the supplied Virtua Fighter 2 v2.2 ROM set:

- ROM manifest matched the expected `epr-18385.12` through `epr-18388.15` i960 program set.
- native-runtime and native-scheduler unit tests pass.
- phase-17 zero differential test passes.
- ROM-backed native second, third, fourth, fifth and sixth dispatch tests pass after the recovery.

The old v0142 patch was intentionally discarded because its source context no longer matched the current tree. This v0143 recovery is a semantic rebase onto the current implementation.
