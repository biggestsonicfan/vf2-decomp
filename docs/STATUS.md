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
| Repeated-cycle differential API | Implemented; CLI/endurance integration pending |
| ROM-independent / ROM-backed CTest targets | 8 / 25, 33 total |
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

This infrastructure is the foundation for a future `native-sixth-dispatch`
contract and a repeated-frame endurance command. It does not move the current
ROM-proven boundary by itself: the accepted boundary remains the fifth
`fa_game_info` entry until the complete fifth sweep has been observed, recovered
and validated against the supported ROM set.

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

This tooling does not extend the ROM-proven boundary by itself. Its first
expected failure identifies the concrete unsupported block that must be
recovered before a strict sixth-dispatch contract can be recorded.
