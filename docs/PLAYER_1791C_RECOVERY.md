# Player `0x1791c` recovery

This note records the clean-room recovery boundary for the player motion routine at `0x0001791c`.

## Recovered scope

The native player bridge no longer requires the single observed zero-motion snapshot for `0x1791c -> 0x17b64 -> 0x14408`.

The generalized planner now recovers the scalar motion pipeline visible in the i960 routine:

- copies the pre-integration position triple to `0x0050e000`;
- derives the primary damping limit from the global tick/scale, the fighter speed scale and the `1/60` constant;
- derives the secondary damping limit from the primary one;
- reduces the primary X/Z vector while preserving direction;
- reduces the secondary X/Z vector by the same norm-preserving rule;
- clears the secondary factor when the secondary vector is exhausted before a primary-vector scale;
- applies the conditional vertical `-tick + bias` adjustment;
- integrates primary velocity into position;
- applies either the full secondary X/Z vector or the runtime-bit-20 half step; and
- stores the resulting movement delta at `player + 0x1e00`.

Planning is transactional: all required reads and finite-value checks happen before the first committed write.

## TGP scalar command `0x16802d2d`

Cross-call evidence identifies `0x16802d2d` as a two-component scalar norm service.

At `0x1791c` the ROM submits `(velocity_x, velocity_z)`, reads one scalar, compares it with a limit, computes `1 - limit / scalar`, and multiplies both vector components by that same factor. It immediately repeats the identical protocol for `(secondary_x, secondary_z)`. Other player call sites use the same two-input / one-output command shape for distance comparisons.

The native recovery therefore models the finite-input operation as:

```text
length = sqrt(x*x + z*z)
```

Non-finite inputs are rejected back to the original path rather than assigned guessed TGP edge semantics.

## Differential evidence

The normal sixth-dispatch ROM-backed differential remains an exact CPU-and-memory match with the generalized `0x1791c` layer active:

- repeated-frame reference instructions: `7,404,917`;
- continuous recovered instructions: `8,675,741`;
- final CPU state: match;
- final memory state: match.

The isolated observed zero case also reaches `0x14408` after exactly `87` instructions, matching the original routine.

Three additional controlled non-zero states were chosen so the project's flat coprocessor-port reference remains mathematically faithful to the recovered norm operation: each tested norm used a vector `(0, z)` with positive `z`, so the last argument stored to the flat port equals `sqrt(0^2 + z^2)`.

| Controlled path | ROM instructions | Final `position.z` | Final `position.y` |
|---|---:|---:|---:|
| full secondary step | 104 | 7.0 | 1.4972777367 |
| runtime bit 20 half step | 109 | 5.0 | 1.4972777367 |
| player flag bit 2 vertical path | 94 | 7.0 | 1.4972777367 |

For these cases the ROM also preserves `velocity.z = 3.0`, `secondary.z = 4.0`, and writes the same final displacement to `player + 0x1e00` as the recovered equations predict.

## Remaining numerical boundary

The command identification is strong, but arbitrary non-axis inputs would require a real TGP response or a recovered command interpreter to establish any hardware-specific approximation/rounding behavior beyond ordinary finite single-precision norm semantics. The native planner deliberately rejects non-finite numerical states and keeps the original i960/TGP path available as the fallback boundary.
