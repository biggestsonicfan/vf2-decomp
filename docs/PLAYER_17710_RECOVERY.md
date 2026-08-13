# Player `0x17710` recovery

This note records the clean-room recovery boundary for the player control-flow routine at `0x00017710`.

## Recovered scope

The native layer follows the conditional tree from `0x17710` through the common return at `0x17918` without requiring one exact fighter snapshot.

Recovered behavior includes:

- state-bit and fighter-flag early exits;
- opponent-state and counter gates;
- the `0x177e0`, `0x177f0`, `0x17800`, `0x17818`, `0x17854`, `0x1785c`, `0x1786c`, `0x17890` and `0x17898` scalar paths;
- signed 16-bit normalization and clamping;
- the `cmpi` / `concmpi` condition-code semantics used by the range test;
- the non-rotation `0x1790c` tail that adds the computed delta to `player + 0x26`;
- the full `0x178bc` planar X/Z rotation tail, including non-zero angles; and
- architectural instruction accounting for each accepted path.

The scalar planner is transactional. It evaluates every branch and required read before committing the optional `player + 0x26` write or condition-code update.

## TGP command `0x2d805b5b`

Cross-call evidence identifies `0x2d805b5b` as a planar X/Z rotation service: callers submit a signed 16-bit angle followed by two scalar components and consume two scalar outputs. The command is reused for fighter offsets and for an arena/object bounds pass that rotates many X/Z pairs, computes extrema, then submits the negated angle to transform the extrema back.

### Rotation orientation

The non-zero sign convention is now recovered from the ROM itself rather than guessed from the project's flat coprocessor-port fallback.

At `0x3a320`, the game submits the two fighter/world positions to command `0x17802f2f` in the order:

```text
z0, z1, x1, x0
```

The recovered TGP atan service establishes the corresponding angular convention as:

```text
theta = atan2(z1 - z0, x1 - x0)
```

The game then adds `0x4000` — one quarter turn — and applies `0x2d805b5b` to a set of X/Z points before computing axis-aligned extrema.

Only one of the two possible planar sign conventions makes the original edge's transverse component vanish after that `theta + 0x4000` rotation:

```text
x' = x * cos(theta) + z * sin(theta)
z' = z * cos(theta) - x * sin(theta)
```

Therefore positive angles rotate **clockwise in the X/Z plane**: `+0x4000` maps `+X` to `-Z`.

The same ROM routine later submits `-theta` to `0x2d805b5b` to transform the extrema back, independently confirming that the recovered matrix has the required inverse convention.

## Angle representation and sine/cosine

Angles are 16-bit turns:

```text
0x0000 =   0 degrees
0x4000 =  90 degrees
0x8000 = 180 degrees
0xc000 = 270 degrees / -90 degrees
```

The TGP's recovered sine/cosine service uses quadrant reduction over a 0x4000-entry first-quadrant lookup ROM. The native player bridge reproduces that integer quadrant/mirroring behavior and exact cardinal values.

The current `vf2_model2a` runtime does not expose the physical TGP table ROM to recovered player helpers. For non-cardinal samples the semantic helper therefore reconstructs the mathematical first-quadrant sample with `sinf` after integer reduction. A local comparison against the V2.2 TGP table found only one-ULP differences at a small minority of table entries; this numerical boundary is documented explicitly rather than hidden behind the flat-port model.

No proprietary lookup table is copied into the repository.

## `0x178bc` execution

The scalar C planner already owns every non-rotation path. If planning determines that live control flow truly falls through to `0x178bc`, the bridge performs a transactional prefix probe:

1. copy the i960 CPU state;
2. execute the read-only `0x17710 -> 0x178bc` prefix on that CPU copy;
3. require an exact stop at `0x178bc`;
4. verify the complete prefix + 23-instruction rotation-tail budget;
5. commit the probed CPU state; and
6. execute the TGP rotation tail semantically in C.

No memory write occurs before `0x178bc`, so a failed probe can be discarded without rollback.

The tail reproduces the ROM sequence:

```text
dx = player.x - base.x
dz = player.z - base.z
(dx', dz') = rotate_clockwise(dx, dz, delta_angle)
player.x = base.x + dx'
player.z = base.z + dz'
player.angle += delta_angle
```

It also preserves the architectural `r3/r4/r5/r6/r13/r14/r15` effects and the final RET.

The dispatcher distinguishes a genuine TGP fall-through from a scalar path rejected only because of a short `max_steps` budget, so a short caller budget cannot accidentally activate the rotation probe.

## Clean-room orientation tests

`tests/recovered/test_player_planar_rotation.c` validates the recovered matrix without a game ROM:

- angle `0`: `(2,3) -> (2,3)`;
- `+0x4000`: `(2,3) -> (3,-2)`;
- `-0x4000`: `(2,3) -> (-3,2)`;
- `0x8000`: `(2,3) -> (-2,-3)`;
- the `0x3a320` edge-alignment proof: `(1,1)` rotated by `0x6000` produces exactly zero transverse X and approximately `-sqrt(2)` on Z; and
- non-finite inputs are rejected before mutation.

These tests run under GCC, Clang and Clang ASan/UBSan.

## Earlier differential evidence

Controlled snapshots generated locally from a real V2.2 runtime state established the scalar control-flow instruction accounting:

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

The superseded `player_i960_bridge_17710_rotation0.inc` helper was removed. Zero and non-zero rotations now share the same semantic matrix implementation.

Repository CI passes with GCC release, Clang release and Clang ASan/UBSan.

The V2.2 ROM-backed `native-sixth-dispatch` validation remains an exact CPU-and-memory match with the full planar-rotation layer composed:

- repeated-frame reference instructions: `7,404,917`;
- continuous recovered instructions: `8,675,741`;
- final CPU state: MATCH;
- final memory state: MATCH.

No ROM image, TGP lookup table or runtime snapshot is committed to the repository.
