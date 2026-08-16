# Repeated native dispatch acceptance through dispatch 256 (v0.1.3)

The recovered i960 runtime was extended beyond the published sixth-dispatch
milestone using the repository's `native-nth-dispatch` differential driver and
the supported 36-file Virtua Fighter 2 Version 2.1 ROM set.

The command

```sh
build/vf2i960 native-nth-dispatch /path/to/vf2 256 dispatch-256.vf2snap
```

starts from the existing strict sixth-dispatch base and advances the recovered
runtime and the reference i960 executor in lockstep until the 256th
`fa_game_info` entry at `0x0001645c`.

## Result

Dispatches 7 through 256 all match exactly:

- 250 additional repeated dispatches;
- 9,000 differential native blocks;
- 406,577 recovered instructions compared against the same number of reference
  instructions;
- 254 repeated scheduler entries by the final checkpoint;
- 765 repeated scheduler transitions;
- 254 repeated scheduler finishes;
- 508 repeated frame-wait phases; and
- complete CPU, local-frame, execution-counter and mutable-memory equality at
  every dispatch boundary.

The final checkpoint is still at `fa_game_info` (`0x0001645c`) with registry
pointer `0x00515200`. No interpreted instruction is needed on the native side.

## Dynamic corridor changes

The long run is not merely the same fixed instruction profile repeated 250
times. It crosses a deterministic state transition while remaining exact.

Dispatches 7-29 alternate between 2,170 and 2,173 recovered instructions per
cycle. Dispatch 30 is 2,167 instructions, and dispatch 31 drops to 1,567. From
there the stable repeated profile alternates around 1,567/1,570 instructions,
with 1,565-instruction cycles at dispatches 33, 65, 97, 129, 161, 193 and 225.
Those 32-dispatch periodic reductions are reproduced identically by the
reference executor.

Every dispatch from 7 through 256 uses 36 differential blocks after the sixth
base. The instruction-profile changes therefore come from recovered conditional
behavior inside existing blocks rather than from fallback interpretation or a
changed validation granularity.

## Why this matters

The previous milestone proved a handful of repeated scheduler sweeps. This run
proves that the same recovered C survives hundreds of scheduler/frame cycles,
including a live state transition and later periodic conditional paths, without
state drift. It materially raises confidence in the recovered frame scheduler,
interrupt return path, repeated task contexts and main-frame bridge.

This remains an observed idle/runtime corridor, not complete gameplay. Character
selection, match progression, fighter physics/collision, broader input modes and
other unobserved task branches remain separate recovery work.
