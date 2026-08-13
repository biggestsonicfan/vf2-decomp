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
- the non-coprocessor `0x1790c` tail that adds the computed delta to `player + 0x26`;
- the `0x178bc` planar-rotation corridor when the clamped command angle is exactly zero; and
- exact architectural instruction accounting for each accepted path.

The planners are transactional. They evaluate every branch and required read before committing the optional `player + 0x26` write, condition-code update or flat-port compatibility write.

## TGP command `0x2d805b5b`

Cross-call evidence identifies `0x2d805b5b` as a planar X/Z rotation service: callers submit a signed 16-bit angle followed by two scalar components and consume two scalar outputs. The command is reused for fighter offsets and for an arena/object bounds pass that rotates many X/Z pairs, computes extrema, then submits the negated angle to transform the extrema back.

The complete non-zero angle convention is not yet claimed as recovered.

The zero-angle subset is, however, unambiguous: any valid planar rotation at angle zero is the identity transform. A controlled state-bit-23 path was constructed with `source_angle - current_angle == 0`, position/base delta `(1.0, 1.0)`, player flag bit 2 set and the `+0x840` gate clear. The ROM reached `0x178bc`, preserved both X/Z coordinates at `1.0`, left `player + 0x26` unchanged, and returned at `0x14404` after exactly 42 instructions.

That evidence now backs a narrow native TGP-facing corridor. The same path with player flag bit 2 clear has a 40-instruction architectural count and shares the recovered zero-angle identity semantics.

## Explicit boundary

A `0x178bc` path whose clamped rotation angle is non-zero is still rejected with `VF2_ERROR_UNSUPPORTED` before mutation. The existing dispatch chain then delegates that state to the earlier bridge / architectural i960 executor.

This keeps the remaining rotation-sign/orientation question explicit instead of assigning guessed coprocessor behavior.

## Differential evidence

Controlled snapshots were generated locally from a real V2.2 runtime snapshot, then executed with the architectural i960 interpreter. No ROM image, snapshot, or proprietary memory dump is stored in the repository.

The generalized scalar planner was checked against these measured endpoints:

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
| state bit 23 zero-angle rotation | 42 | unchanged | unchanged | 0 |

The controlled endpoints match the ROM interpreter instruction count, status word and condition-code behavior for the covered branch family.

## Integration validation

The recovered layers are inserted in the hybrid dispatch chain while preserving fallback behavior.

The repository CI passes with GCC release, Clang release, and Clang ASan/UBSan. The ROM-backed `native-sixth-dispatch` validation also remains an exact CPU-and-memory match with the zero-angle layer wired: 7,404,917 repeated-frame instructions were compared and the continuous recovered run reached 8,675,741 instructions with matching final state.
