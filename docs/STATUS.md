# Status

## Current master — v0.1.3

| Component | State |
|---|---|
| Repository and warning-as-error C17 build | Validated locally; GCC/Clang CI configured |
| Supported ROM validation | 36/36 files |
| Accepted startup/post-frame bridge | Recovered C |
| Post-frame bridge instructions | 1,270,822 / 1,270,822 recovered |
| Repeated-frame corridor | 7,402,741 instructions across 830 differential blocks |
| Continuous recovered instructions | 8,673,563 |
| Native-side interpreted instructions on accepted paths | 0 |
| Second through fourth scheduler sweeps | Completely recovered for the observed corridor |
| Fifth scheduler entry | Recovered and ROM-validated |
| Current ROM-proven native boundary | Fifth `fa_game_info` at `0x0001645c` |
| `vf2i960 native-third-dispatch` | `MATCH`: 42 blocks / 55,239 instructions |
| `vf2i960 native-fourth-dispatch` | `MATCH`: 78 blocks / 58,869 instructions |
| `vf2i960 native-fifth-dispatch` | `MATCH`: 830 blocks / 7,402,741 instructions |
| Repeated-cycle differential API | Strict per-block + cycle-boundary probe + resumable checkpoints |
| ROM-independent / ROM-backed CTest targets | 11 / 25, 36 total |
| TGP protocol and renderer | Not recovered |
| Motorola 68000 / SCSP audio | Not recovered |
| Window, input and production platform backend | Not implemented |
| Playable port | No |

## Development head after v0.1.3 acceptance

The differential layer now exposes `vf2_native_differential_run_cycles`, which
runs any requested number of complete lockstep cycles back to the same task
entry. Each non-zero cycle enforces a minimum block count, preventing an
immediate false success when the start and destination addresses are identical.
The aggregate report preserves completed-cycle, block and instruction totals as
well as the partial final cycle when an unsupported state or mismatch is found.

This infrastructure now backs both the strict repeated-frame endurance command
and the faster cycle-boundary probe. It does not move the strict ROM-proven
per-block boundary by itself: probe-only cycles are scouting evidence until the
corresponding interval is replayed under the strict differential contract.

## Proven scope

The native path runs continuously from the completed first scheduler sweep
through the original 1,270,822-instruction post-frame bridge, the complete
observed second, third and fourth scheduler sweeps, three repeated frame
boundaries and the fifth scheduler entry. The fifth `fa_game_info` task is
reached at `0x0001645c`, registry `0x00515200`, without native-side i960
interpretation.

The v0.1.3 differential contract is:

- 830 compared repeated-frame blocks;
- 7,402,741 reference and recovered-native instructions;
- 8,673,563 continuous recovered instructions including the historical bridge;
- three repeated scheduler entries and finishes;
- twelve repeated scheduler transitions;
- six frame-wait phases; and
- exact CPU, architectural local-frame, execution-counter, frame-event and all
  mutable Model 2 memory equality after every accepted block.

The extension beyond v0.1.2 includes the observed:

- texture status scan with nine inactive records before the live tenth record;
- non-zero texture stream dispatch and five mip levels;
- 43,648 source bytes expanded into 87,296 texture bytes;
- zero-value texture-counter completion path;
- stream-resume gate with no pending transfer; and
- mode-17 system-memory diagnostic profile, which executes one instruction more
  than adjacent mode 16.

Run the strict acceptance with:

```sh
vf2i960 native-fifth-dispatch /path/to/vf2
```

The older third- and fourth-dispatch commands remain independent regression
contracts.

## Validation

The exact supported 36-file ROM set passes all 33 configured CTest targets:
8 ROM-independent tests and 25 ROM-backed differential/observation tests. The
ROM-independent suite also passes under AddressSanitizer,
UndefinedBehaviorSanitizer and LeakSanitizer.

Public CI cannot contain the proprietary ROM set, so GitHub Actions covers the
warning-as-error GCC/Clang builds and sanitizers while strict ROM-backed
acceptance is run locally against legally obtained files.

The reference i960 executor remains a differential oracle only. It advances by
the exact instruction count reported by each recovered block and never supplies
state to the native side. Unknown addresses and unobserved branches continue to
return `VF2_ERROR_UNSUPPORTED`.

## Roadmap position

v0.1.3 completes the extension from the fourth scheduler entry to the fifth.
The active stage remains v0.2.0 game subsystems. The immediate execution target
is the complete fifth scheduler sweep and longer repeated-frame endurance runs,
while replacing evidence-specific raw-address logic with fighter, object,
match, animation, collision and input types.

TGP rendering, 68000/SCSP audio and a production platform backend remain later
milestones.

## Endurance tooling

The differential layer now has two reusable pieces for the next v0.2.0 step:

- `vf2i960 native-fifth-dispatch ROM_DIR OUTPUT.vf2snap` persists the exact
  validated fifth-`fa_game_info` boundary; and
- `vf2cycles` restores that boundary and executes a requested number of complete
  repeated-address cycles with per-block CPU and mutable-memory comparison.

The checkpoint now includes a versioned, CRC-protected runtime-state sidecar
covering frame-wait progress and all native aggregate counters. The endurance
runner performs public one-block differential steps and can persist the last
fully matched state immediately before a failure.

ROM-backed endurance now extends the proven corridor substantially beyond the
fifth entry. A verified 36/36 ROM set completed 1,000 additional repeated-
address cycles with exact CPU, local-frame, counter, frame-event and mutable-
memory equality: 36,000 additional native blocks and 1,582,507 recovered i960
instructions. The final checkpoint is again `fa_game_info` at `0x0001645c`,
with 1,003 scheduler entries and 1,003 injected frame IRQs accumulated by the
native runtime.

That endurance run exposed and fixed two previously unobserved periodic paths:

- inactive palette upload at `0x00002de4` now preserves the incoming i960
  arithmetic condition because the ROM's `cmpobe` does not modify condition
  codes; and
- the frame-timer prefix at `0x00010f08` now accepts the
  `(frame_counter & 31) == 0` path, skips the previous-minimum load/comparison
  exactly like the ROM, and accounts the 21-instruction path.

`vf2cycles` now also exposes `--boundary-probe` for long-horizon scouting. The
reference executor still advances by the recovered instruction count of every
native block and frame-wait host state is checked when it changes, but complete
CPU/mutable-memory snapshots are compared only when the repeated address closes
the cycle. A failed probe restores both machines and the runtime sidecar to the
exact beginning of the failing cycle, ready for strict per-block replay.
`--output-snapshot` persists a successful boundary plus its `.runtime` sidecar,
so long probes can resume without replaying earlier cycles.

Using the verified ROM set, cycle-boundary probing reached scheduler entry and
frame IRQ 16,384 with complete cycle-end state equality. This does **not** extend
the strict per-block acceptance claim beyond the published 1,000-cycle corridor.
It does show that passive repeated-frame execution has settled into a stable
state: the observed mode remains `0x00020000`, phase state/index remain
`0xff`/`0x0b`, and gameplay flags remain zero.

Controlled phase-navigation transitions are now recovered in both directions.
Injecting gameplay bit 13 (`0x00002000`) at the exact pre-`main-final-cluster`
boundary exposes the phase-17 step-back branch in `0x00058fe0`: the reference
i960 decrements phase index `11 -> 10`, writes `0x8020` to the previous
double-indirect phase target and `0x801c` to the new target, and executes 49
frame-dispatch instructions. The `0 -> 11` wrap variant takes 50 instructions.
Injecting a forward-mask bit from `0x08001008` proves the symmetric direction:
`11 -> 0` wraps in 50 instructions and ordinary `10 -> 11` takes 49, with the
forward path taking ROM-accurate priority if bit 13 is also set. Strict replay
now matches the enclosing 281-instruction step-back and 282-instruction forward
`main-final-cluster` variants plus their complete following 36-block scheduler
cycles (2,185 and 2,186 instructions respectively). This is controlled
state-transition evidence, not a claim that passive natural execution reaches
these branches.

The phase-17 reset/display mask `0x04000104` is now recovered from the same
controlled boundary. The ROM sets phase-index bit 7, clears the auxiliary phase
byte, latches `0xff`, zeroes the current object marker, clears the 48x64 tile
plane to `0x0020`, and centers/copies the current phase label. The recovered
dispatch accounts 12,657 instructions with 5 calls / 6 returns; the complete
12,889-instruction `main-final-cluster` is strict-equal, followed by 35 more
equal recovered blocks. The first subsequent failure is the expected phase-index
bit-7 indirect dispatch at `0x00059154`.

The next high-value v0.2.0 target is therefore that observed bit-7 indirect
phase dispatch, followed by controlled evidence for phase-state-zero, additional
match/selection and fighter/object paths rather than unbounded passive endurance
in the same attractor.
