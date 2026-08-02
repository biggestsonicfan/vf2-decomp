# v0.0.21 second scheduler-entry evidence

## Accepted interval

- main-loop call site: `0x0000a010`;
- scheduler procedure: `0x00010d54`;
- selected task call site: `0x00010dc8`;
- selected task entry: `0x0001645c`;
- selected registry: `0x00515200`;
- task index: 13;
- recovered instructions: 235;
- recovered procedure calls / returns: 4 / 2.

## Descriptor scan

The observed second pass starts at registry `0x00510000` and scratch
`0x0050c000`. Descriptors 0 through 12 are inactive. Their timing scratch word
at offset `+0x10` is updated while the registry cursor advances by each
descriptor's recorded stack size and the scratch cursor advances by `0x20`.
Descriptor 13 is the first runnable entry and resolves to `fa_game_info`.

## Architectural post-state

Before the task `callx`, the scheduler frame contains task count 29, current
index 13, timer reload values `0x000fffff`, registry `0x00515200` and scratch
`0x0050c1a0`. The recovered implementation enters a fresh task frame and leaves
the scheduler continuation cached at `0x00010dcc`. The reference and recovered
machines match in CPU state, local frames, counters and all 18 mutable memory
regions at the task entry.

## Claim boundary

This is the observed second-entry path only. Alternate ready-state branches,
different runnable selections and later second-pass task execution remain
unsupported rather than generalized without evidence.
