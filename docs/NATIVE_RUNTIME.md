# Recovered native runtime

## Purpose

`vf2_native_runtime` is the reusable execution layer above individual recovered
blocks. It is independent from the ROM-backed differential CLI. Recovered
blocks run natively; the fighter-state bit-31 branch has one explicit,
interpreter-backed task bridge while its C recovery is still open.

It routes accepted bridge, task, frame-wait, interrupt and scheduler states,
uses live task-registry strides, preserves persistent task contexts and reports
unknown or unobserved paths as `VF2_ERROR_UNSUPPORTED`.

## Fighter-state bridge

When either fighter record carries bit 31, `fa_game_info` selects the ROM
updates at `0x00018144` and `0x00018644`. The runtime now executes that task
through the reference i960 executor until the architectural `RET` reaches the
scheduler at `0x00010dcc`, preserving exact CPU, frame and mutable-memory
behavior. This is a runtime completion bridge, not a claim that those large
fighter procedures have been translated to native C.

The following scheduler task at `0x00013f08` is also routed through the same
explicit bridge as `fa_player`. A real sixth-entry snapshot with both fighter
bit-31 flags forced now advances from `fa_game_info` through the player tasks
and returns to `0x0000a014`; the native-resume command is the reproducible
smoke test for that continuation:

```sh
vf2i960 native-resume /path/to/vf2 sixth-entry.vf2snap 20 0x80000000 0xa014
```

## API

```c
vf2_native_runtime_state runtime;
vf2_status status = vf2_native_runtime_initialize(&runtime, 4u);

vf2_native_runtime_step_report step;
status = vf2_native_runtime_step(&machine, &cpu, &runtime, &step);

vf2_native_runtime_run_report run;
status = vf2_native_runtime_run_until(
    &machine,
    &cpu,
    &runtime,
    stop_address,
    max_blocks,
    &run
);
```

Repeated cycles begin and end at the same `fa_game_info` address. The native
differential layer therefore exposes a minimum-block stop contract so the
current address is not mistaken for completion of another cycle.

## Accepted fifth-dispatch corridor

The v0.1.3 runtime composes:

1. the complete observed second and third scheduler sweeps accepted previously;
2. the fourth scheduler entry;
3. the complete observed fourth scheduler sweep;
4. a large texture-orchestrator interval with a non-zero compressed stream;
5. the next geometry, game, timer, interrupt and frame-wait paths; and
6. the fifth `fa_game_info` entry at `0x0001645c`.

The strict command is:

```sh
vf2i960 native-fifth-dispatch /path/to/vf2
```

It requires exactly:

- **830** repeated-frame differential blocks;
- **7,404,913** reference and recovered-native instructions;
- **8,675,735** continuous recovered instructions including the historical
  bridge; and
- zero native-side interpreter fallbacks.

`native-third-dispatch` and `native-fourth-dispatch` remain separate regression
contracts at 42 / 55,239 and 78 / 58,869 respectively.

## Newly accepted texture interval

The fourth sweep exposes the first observed non-zero stream at `0x0004be6c`.
The recovered C path validates the header and timer preconditions, expands five
mipmap levels and reproduces the measured CPU and memory poststate:

- **35,059** instructions;
- two nested procedure calls and two returns;
- **43,648** source bytes consumed;
- **87,296** texture bytes written; and
- 5 levels with decreasing 128, 64, 32, 16 and 8 row counts.

Each 16-byte source group is copied to the destination and followed by four
32-bit values containing the upper half of each source word. Destination rows
use the observed `0x800` stride. Header variants, malformed dimensions and
unobserved timer states remain explicitly unsupported.

The same corridor also recovers a texture-record scan with nine inactive
records before the live record, the no-pending stream resume gate and the
zero-counter completion path.

## Accounting and validation

Per-step and cumulative reports retain entry/exit addresses, block/task kinds,
scheduler indices and registry addresses, descriptor scans, frame-wait phases,
instruction counts and procedure calls/returns. Cumulative state is updated only
after an accepted runtime step succeeds.

The reusable differential runner advances the reference i960 by the exact
reported native instruction count, mirrors deterministic host frame events and
compares complete CPU state, local frames and all mutable Model 2 memory after
every block.

## Native game-frame boundary

`vf2_game_attach_native_runtime` can now be paired with the software graphics
and sound backends. During a native frame, Model 2A coprocessor-port writes are
captured as an ordered stream: function-port writes receive the Model 2 address
tag and FIFO writes remain raw words. The stream is handed to the existing TGP
geometry decoder before the software frame is closed. The capture deliberately
falls back to the Model 2A flat backing store after observing each write, so
the still-incomplete TGP device does not perturb the recovered runtime.

The native frame boundary reads the observed geometry FIFO ring interval
and executes the command classes currently understood by the TGP reference
decoder; unrecognized or empty object data remains non-fatal and produces no
triangles.

This is an integration boundary, not a claim of a playable game: gameplay input
semantics, the complete TGP packet/microcode protocol, gameplay state and full
sound behavior remain open.

The Model 2A input facade now exposes the active-low 315-5649 B/C/D input
ports at `0x01c00002`, `0x01c00004` and `0x01c00006`. `vf2_game_set_input`
updates the platform surface and synchronizes the native machine, including
VF2 P1/P2 joystick, punch, kick and guard controls, plus start, coin and
service controls.

The current CMake configuration exposes 16 ROM-independent and 26 ROM-backed
tests. The focused geometry, game and native-runtime targets pass; the full
optimized ROM-backed suite still has known access violations in the unfinished
native TGP/gameplay paths. The ROM-independent suite passes under ASan,
UBSan and LeakSanitizer.

## Next integration

The current native boundary includes the observed post-boot palette build at
`0x00002c38`; its 28-by-32 RGB ramp, page latch and `0x00020050` return stub
are covered by a synthetic state regression. The resumed `0x0001fe64` wrapper
prefix is now also recovered through its `0x4b410` registration helper, state
clears, and call into `0x0002eab8`; that helper's 90-instruction state
initializer and nested `0x31004` setup are now covered as well. Scouting from
the resumed path now also covers its `0x1fedc` call into the ROM's `0x11704`
byte-to-luma-table copier, including live G0/G1/G2 pointer/count poststate and
the trailing `0x1fee0` return. The frame-dispatch bridge now covers selectors
0, 1 and 2, including the ROM `0xab0c` control-channel reset and selector-3
handoff; selector 3's mode-table worker remains the next native boundary.
Longer endurance runs remain after that branch. Each new branch should retain an exact differential
contract and a synthetic state regression where practical before broader
fighter, object, match, animation, collision and input types are introduced.
