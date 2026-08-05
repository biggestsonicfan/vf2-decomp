# Status

## Current master — v0.1.2

| Component | State |
|---|---|
| Repository and warning-as-error C17 build | Validated locally; GCC/Clang CI configured |
| Supported ROM validation | 36/36 files |
| Accepted startup/post-frame bridge | Recovered C |
| Post-frame bridge instructions | 1,270,822 / 1,270,822 recovered |
| Repeated-frame corridor | 58,869 instructions across 78 differential blocks |
| Continuous recovered instructions | 1,329,691 |
| Native-side interpreted instructions on accepted paths | 0 |
| Second and third scheduler sweeps | Completely recovered for the observed corridor |
| Fourth scheduler entry | Recovered and ROM-validated |
| Current ROM-proven native boundary | Fourth `fa_game_info` at `0x0001645c` |
| `vf2i960 native-third-dispatch` | `MATCH`: 42 blocks / 55,239 instructions |
| `vf2i960 native-fourth-dispatch` | `MATCH`: 78 blocks / 58,869 instructions |
| ROM-independent / ROM-backed CTest targets | 8 / 24, 32 total |
| TGP protocol and renderer | Not recovered |
| Motorola 68000 / SCSP audio | Not recovered |
| Window, input and production platform backend | Not implemented |
| Playable port | No |

## Proven scope

The native path now runs continuously from the completed first scheduler sweep
through the original 1,270,822-instruction post-frame bridge, the complete
observed second and third scheduler sweeps, two repeated frame boundaries and
the fourth scheduler entry. The fourth `fa_game_info` task is reached at
`0x0001645c`, registry `0x00515200`, without native-side i960 interpretation.

The v0.1.2 differential contract is:

- 78 compared repeated-frame blocks;
- 58,869 reference instructions and 58,869 recovered native instructions;
- 1,329,691 continuous recovered instructions including the historical bridge;
- two repeated scheduler entries and finishes;
- nine repeated scheduler transitions;
- four frame-wait phases; and
- exact CPU, architectural local-frame, execution-counter, frame-event and all
  mutable Model 2 memory equality at every checkpoint.

The extension beyond v0.1.1 includes the observed:

- player-update bit-14 immediate-return branch;
- active selector path in `game_input_update`, followed by both sequence gates;
- active selector immediate-return path in `game_state_update`;
- frame-mode 16/17 memory-diagnostic return after the memory probe;
- active-selector fast return in the frame counter;
- phase-17 frame dispatcher and its simple player setup helper path; and
- late-sweep task and texture-expiration cases already hardened in v0.1.1.

The player-layer composite now executes nested CPU changes against a candidate
state and commits only after both recovered children succeed. The video-layer
commit reads and validates all observed inputs before its first write. These are
specific atomicity protections for the recovered rejection points, not a claim
that every runtime block is globally transactional.

Run the strict acceptance with:

```sh
vf2i960 native-fourth-dispatch /path/to/vf2
```

The older `native-third-dispatch` contract remains unchanged at 42 blocks and
55,239 instructions.

## Validation

The exact supported 36-file ROM set passed all 32 configured CTest targets:
8 ROM-independent tests and 24 ROM-backed differential/observation tests. The
ROM-independent suite also passed under AddressSanitizer, UndefinedBehaviorSanitizer
and LeakSanitizer.

Public CI cannot contain the proprietary ROM set, so GitHub Actions covers the
warning-as-error GCC/Clang builds and sanitizers while strict ROM-backed
acceptance is run locally against legally obtained files.

The reference i960 executor remains a differential oracle only. It advances by
the exact instruction count reported by each recovered block and never supplies
state to the native side. Unknown addresses and unobserved branches continue to
return `VF2_ERROR_UNSUPPORTED`.

## Roadmap position

v0.1.2 completes the first full extension from the third scheduler entry to the
fourth. The active stage remains v0.2.0 game subsystems. The immediate execution
target is the complete fourth scheduler sweep and the next genuinely new state
transition, followed by longer multi-frame endurance runs.

Broader fighter/object types, match logic, animation, collision and input remain
the central v0.2.0 work. TGP rendering, 68000/SCSP audio and a production
platform backend remain later milestones.
