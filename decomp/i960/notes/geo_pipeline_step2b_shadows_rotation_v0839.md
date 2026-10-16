# geo_pipeline_step2b_shadows_rotation_v0839 — shadows + rotation convention

**Geometriser pipeline, Step 2b** (follows `geo_pipeline_step2_matrices_v0838.md`;
kazuyette's visual review of scene.obj frame 20: decor credible, two fighter groups
in credible places, but many flat pieces on the ground around each fighter and
the limbs are disjoint / mis-oriented though roughly positioned). Two measured
checks were added to `vf2i960 geo-scene`, no new command.

## 1. Shadows — flattened matrices [MEASURED]

VF2 draws a fighter's shadow by flattening geometry onto the ground, so a shadow
object's matrix projects onto a plane: its 3x3 part becomes rank-deficient and
`det == 0`. Measure a scale-free **flatness** = `|det(3x3)| / (|c0||c1||c2|)`
(columns `(m0,m1,m2),(m3,m4,m5),(m6,m7,m8)` per MAME `transform_point`): ~1 for a
rotation at any uniform scale, ~0 when the three axes are coplanar.

Result on `vf2_fight_ram9_p1move_30f` frame 20 — a **perfectly clean split**:
- **30 / 113** slots have flatness `<= 0.0000` (exactly flat) = shadows;
- **every** non-shadow slot has flatness `>= 1.0000`.
  No slot sits between — `GS_SHADOW_EPS = 0.10` is uncritical.

So the "flat pieces on the ground" kazuyette saw are the **per-limb ground
shadows** (VF2 flattens each body part, not one blob per fighter). The anchor
cluster `0x130a44..0x1434a0` contributes 3 of them.

**Repartition of the 113:**

| group | count |
|---|---|
| shadows (flatness ~ 0) | **30** |
| non-shadow, camera-common (motion_t <= 0.2498) | **47** |
| non-shadow, self-moving (motion_t >= 0.2498) | **36** |

(`band` + `shadow` + `flatness` columns are in `scene_groups.csv`; shadow slots
are tagged `shadow` in the `o` names in scene.obj.)

## 2. Rotation convention — as-is vs transposed [MEASURED]

MAME `geo_matrix_write` (model2_v.cpp:2007) reads the 12 words straight into
`matrix[0..11]` and translate into `matrix[9..11]` — **identical** to the order
the walker captures, so the 12-value order is not in question. The only question
is whether the as-applied 3x3 is the rotation or its transpose.

`geo-scene` now exports **Akira's group (anchored, shadows removed)** twice —
`akira_asis.obj` (MAME convention, `x' = x*m0 + y*m3 + z*m6 + m9`) and
`akira_transposed.obj` (3x3 transposed, translation kept) — and measures the
**articulation gap** = mean over pieces of each piece's closest vertex distance
to any other piece (small = the limbs join).

Result (15 pieces, 1656 verts each):
- **as-is:** joint gap mean **0.1158**, max 0.2651
- transposed: joint gap mean 0.1421, max 0.2701

**as-is joins the pieces better** (confirms MAME's non-transposed convention and
the value order). But the margin is modest and a ~0.12 joint gap is **not** small:
on Model 2 the host CPU composes the joint hierarchy into each per-object world
matrix, so the limbs should already join almost as in-game. A residual that large
is therefore not expected from the convention and signals a real decoding problem,
not a harmless seam — so this rotation test does not settle the question. (Step 2c
found the cause: the objects need MAME's focus + perspective projection to assemble,
and the scatter was in world/vertex decoding, not the rotation.)

## Build / provenance

Posing added an `mtx` argument (and an optional posed-vertex accumulator for the
gap metric) to `ext_emit_piece`/`ext_emit_tri`; NULL = identity, so
export-pieces/export-textured output is byte-unchanged. gcc and clang produce
byte-identical scene.obj / akira_*.obj / scene_groups.csv; the synthetic walker
test still passes on both. Outputs live in `..\snap\scene` (never committed).

## Next — Step 3

Recover the shared camera/view transform (the motion common to the camera-common
band) and remove it; then classify each **non-shadow** object by its camera-free
residual motion (static = scenery, moving = fighter), and split the fighters
spatially. Shadows (the 30 flattened slots) are already separable and attach to
whichever fighter they sit under. Ground truth: anchor `0x130a44` -> fighter.
