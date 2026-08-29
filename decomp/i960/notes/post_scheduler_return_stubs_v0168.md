# v0168: recover post-scheduler return stubs

The strict `native-second-dispatch` audit still contained two single-instruction
reference-executor steps despite the surrounding post-scheduler corridor being
recovered. Instrumenting only the fallback path identified both as ordinary
architectural `ret` instructions:

- `0x0004bab4 -> 0x00000c0c`, returning from the texture upload dispatcher to
  the interrupt initial cluster;
- `0x000020ec -> 0x00000c94`, returning from the game-state update helper to
  the interrupt input-ring continuation.

Both return edges were already bounded by recovered procedure frames. They are
now explicit one-instruction `return-stub` bridges that execute through
`vf2_i960_cpu_return_procedure`. Keeping the returns as independent bridges is
important: folding either return into its parent block changes the differential
boundary and causes the repeated runtime to advance the i960 oracle by one
instruction too far.

The ROM-backed audit also exposed path-sensitive game-state boundary state. The
selector-mask fast return (`selector_mask & 0x00030000 != 0`) reaches
`0x000020ec` with LESS, while the accepted normal paths reach the same return
stub with GREATER. The short selector and meter-only branches now count only
the instructions actually executed before the pending `ret`; the recovered
return bridge accounts for the final instruction separately.

ROM-backed validation against the supported VF2 set is exact:

- post-scheduler bridge instructions: 1,270,824 total;
- recovered bridge instructions: 1,270,824;
- interpreted bridge instructions: 0;
- recovered bridge blocks: 192;
- recovered bridge calls/returns: 342/340;
- repeated-dispatch structural blocks: third 44, fourth 82, fifth 836, sixth
  874; instruction totals are unchanged;
- second through seventh dispatch tests: exact CPU and mutable-memory match.

No ROM bytes, snapshots, traces or generated proprietary artifacts are
committed.
