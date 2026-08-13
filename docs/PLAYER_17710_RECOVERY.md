# Player `0x17710` recovery

This note records the clean-room recovery boundary for the player control-flow routine at `0x00017710`.

## Recovered scope

The native layer now follows the conditional tree from `0x17710` through the common return at `0x17918` without requiring one exact fighter snapshot.

Recovered behavior includes:

- state-bit and fighter-flag early exits;
- opponent-state and counter gates;
- the `0x177e0`, `0x177f0`, `0x17800`, `0x17818`, `0x17854`, `0x1785c`, `0x1786c`, `0x17890` and `0x17898` scalar paths;
- signed 16-bit normalization and clamping;
- the `cmpi` / `concmpi` condition-code semantics used by the range test;
- the non-coprocessor `0x1790c` tail that adds the computed delta to `player + 0x26`; and
- exact architectural instruction accounting for each accepted path.

The planner is transactional. It evaluates every branch and required read before committing the optional `player + 0x26` write or condition-code update.

## Explicit boundary

A path that actually falls through to the TGP command sequence beginning at `0x178bc` is still rejected with `VF2_ERROR_UNSUPPORTED` before mutation. The existing dispatch chain then delegates that state to the earlier bridge / architectural i960 executor.

This is intentionally narrower than pretending the coprocessor protocol is recovered.

## Differential evidence

Controlled snapshots were generated locally from a real V2.2 runtime snapshot, then executed with the architectural i960 interpreter. No ROM image, snapshot, or proprietary memory dump is stored in the repository.

The generalized C planner was checked against those measured endpoints:

| Case | Reference instructions | `player+0x26` after | compare result | AC condition bits |
|---|---:|---:|---|---:|
| state bit 27 early exit | 8 | unchanged | unchanged | 0 |
| fighter flag bit 22 early exit | 10 | unchanged | unchanged | 0 |
| opponent bit 28, counter 5 | 13 | unchanged | unchanged | 0 |
| opponent bit 28, counter 10, no modes | 16 | unchanged | unchanged | 0 |
| nested opponent bits 8/4 exit | 19 | unchanged | unchanged | 0 |
| state bit 18 early exit | 20 | unchanged | unchanged | 0 |
| state bit 1 + opponent bit 15 exit | 29 | unchanged | unchanged | 0 |
| `r5 == 1` range early exit | 43 | 1000 | LESS | 4 |
| state bit 2 scalar update | 46 | 90 | LESS | 4 |
| state bit 23 scalar update | 23 | 1000 | unchanged | 0 |

All ten cases matched the interpreter instruction count, status word and condition-code endpoint.

## Integration validation

The generalized layer is inserted in the recovered dispatch chain ahead of the older player bridge, while preserving fallback behavior.

The repository CI passes with GCC release, Clang release, and Clang ASan/UBSan. The ROM-backed `native-sixth-dispatch` validation also remains an exact CPU-and-memory match: 7,404,917 repeated-frame instructions were compared and the continuous recovered run reached 8,675,741 instructions with matching final state.
