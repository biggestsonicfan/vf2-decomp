# Player `0x4b640` state-machine recovery

This note records the generalized recovery of the player routine rooted at `0x0004b640` and its caller continuation through `0x00014414`.

## Previous limitation

The first native bridge for this routine was tied to one observed state:

- a fixed player address;
- `countdown == 1`;
- exact table pointers;
- exact record selectors and record contents; and
- an exact 123-instruction path.

That was useful as a differential foothold but it did not represent the routine's actual state machine.

## Recovered behavior

The generalized planner now follows the ROM control flow from live state and handles:

- the state-25 conditional reload through the table at `0x020062d4 / 0x02006308`;
- runtime and mode/submode early-return gates;
- zero-countdown return;
- countdown decrement;
- signed interpolation of the two 16-bit state values;
- the first lookup table's scaled index and player `+0x48` selection;
- side/state dependent lookup-bank selection;
- both record-publisher wrappers;
- record metric refresh, selector refresh and flag publication;
- the global record-dirty/clear behavior used by `0x4b9b8`; and
- the terminal state reload through `player + 0x6b8`.

The planner is transactional: the state/table/record decisions and required memory reads are completed before the first write is committed.

## Recovered helper semantics

The small `0x4b604` helper is modeled as a declarative state load. A non-null source supplies:

- countdown;
- two target signed values;
- the two lookup-table pointers; and
- the next state pointer.

The two wrappers around `0x4b9b8` are modeled as record publication rather than as one hard-coded record image. The C path preserves the ROM's metric/selector short-circuit, the `g3 == 2` record field and the observed global-clear condition.

Selectors above the recovered direct-publisher range still reject before mutation rather than guessing the indirect diagnostic call used by the ROM.

## Instruction accounting

Controlled local snapshots were derived from a real V2.2 entry state and were not committed to the repository.

Three materially different ROM paths were measured:

| State | Reference instructions to `0x14414` | Result |
|---|---:|---|
| countdown `0` | 14 | immediate state-machine return |
| countdown `1` | 123 | final interpolation target, two record publications, terminal reload |
| countdown `3` | 126 | intermediate signed interpolation plus record publication |

The generalized planner reproduces those path lengths. During development this matrix exposed a one-instruction double count of the first wrapper CALL; the accounting was corrected before acceptance.

For the countdown-1 reference, the ROM endpoint has the expected published selectors `0x2b / 0x2c`, record flags `0x10 / 0x30`, `g1..g4 = 0x30, second-record, 2, 0`, and the terminal state pointer in `g0`.

For the countdown-3 reference, the signed interpolation moves `+0x6a8` from `100` toward `300` to `200`, and `+0x6aa` from `-100` toward `100` to `0`, while preserving the same table-driven record semantics.

## Outer-run composition

The new layer is not restricted to an exact `stop_address == 0x14414` request. After recovering the function and the caller's `0x1440c -> 0x14414` branch, it can continue the remaining outer run through the previous hybrid chain with the remaining instruction budget.

This allows the normal player-task run to exercise the generalized fast path instead of bypassing it simply because the caller requested a farther stop.

## Integration validation

Repository CI passes with GCC release, Clang release and Clang ASan/UBSan.

With the generalized `0x4b640` layer composed into the player chain, the V2.2 ROM-backed sixth-dispatch validation remains exact:

- repeated-frame reference instructions: `7,404,917`;
- continuous recovered instructions: `8,675,741`;
- final CPU state: MATCH;
- final memory state: MATCH.

No ROM image or controlled runtime snapshot is stored in the repository.
