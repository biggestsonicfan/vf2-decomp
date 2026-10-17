# geo_pipeline_step2_matrices_v0838 — per-object matrices, bands, posed scene

> **CORRECTION (v0857/v0858):** this note (and v0834/v0836/v0837/v0839) calls
> `0x130a44` "Akira's face / anchor". That was a wrong early guess. `0x130a44` is
> **Lau's** face -- separated by projected screen position since v0841, and in
> `lau.obj` since v0853. Fighters are split by 3D position, never by record range.

**Geometriser pipeline, Step 2** (plan in `geo_pipeline_step0_sm2emu_review_v0836.md`,
walker in `geo_pipeline_step1_walker_v0837.md`). Extracts every object's transform
matrix across the 30 RAM9 frames, groups the 113 objects by how their matrix
behaves, and writes a posed full-scene OBJ for an independent eyeball of the
object count and layout.

## Tool

`vf2i960 geo-scene <ram9.bin> <readstarts.txt> <rom-dir> <tex0> <tex1>
<pose-frame> <out-dir>` (local-only, never committed; the walker it calls is
covered synthetically in CI). It walks all 30 frames with `vf2_geo_walk`,
tracks each object slot's 12-float matrix per frame, classifies, and writes
`scene.obj` + `scene.mtl` + `scene_groups.csv` + `tex0/1.png` into out-dir.

Geometry for the posed scene is decoded from the polygon ROM via the **same
validated path as export-pieces** (`vf2_model_lookup_w0(tpa)` -> record offset,
UV/texheader from the `0xe0004 + id*16` table), then each vertex is transformed
by the object's matrix with MAME's convention
(`third_party/mame-model2-ref/model2_v.cpp:112` `transform_point`:
`x' = x*m0+y*m3+z*m6+m9`, etc.). `ext_emit_piece`/`ext_emit_tri` gained an
`mtx` argument (NULL = identity, so export-pieces/export-textured output is
unchanged).

## Measured result — the camera is in every matrix [MEASURED]

Walking `vf2_fight_ram9_p1move_30f` (pose frame 20, the kick):

- **Object count:** 113 per frame, **stable across all 30 frames** (independent
  re-count, confirms Step 1).
- **0 / 113** slots have a byte-identical matrix across all frames. The camera
  pan is composed into **every** object's matrix, so a raw static-vs-moving test
  does **not** split decor from fighters. The earlier working assumption that stage
  matrices "stay fixed while the camera pans" is **false for the raw display-list
  matrices** — measured, not asserted.
- What the raw matrices *do* give is two clean bands in `motion_t` (max
  frame-to-frame translation deviation), split at the widest gap in the sorted
  data (0.2498, a real gap between 0.2275 and 0.2720 — genuinely bimodal):
  - **camera-common = 61 slots** (motion_t <= 0.2498): ride the camera only —
    candidate scenery *and* slow-moving fighter parts, indistinguishable here.
  - **self-moving = 52 slots** (0.272..0.596): move on their own this frame —
    both fighters' animated parts (walk + punch/kick).
- **Akira anchor** cluster `0x130a44..0x1434a0` (obj 0x19c = his face): 18 slots,
  up to motion_t 0.596; **7 are self-moving** (his extended limbs this frame),
  the rest sit at the camera floor (torso/standing parts). So Akira's group
  demonstrably moves — but so does the opponent, and much of each fighter rides
  the camera floor alongside the stage.

**Conclusion:** the full decor / fighter-1 / fighter-2 / shadows split kazuyette
asked for is **not recoverable from the raw matrices** — the camera must be
recovered and removed first, then per-object *residual* motion tests the split.
That is exactly Step 3. Step 2 delivers the matrices, the measured camera finding
(0/113 fixed; bimodal bands), and the posed scene for the count check.

## Posed scene OBJ — independent count/layout control

`scene.obj`: all **113** object slots of the pose frame, each `o
slotNNN_<band>[_akira]_obaXXXXXX`, posed by its matrix, geometry+texture from
ROM — **113/113 resolved, 0 stubs**, 4575 verts / 1525 tris. Open with the
fit-to-bounds viewer to compare the on-screen object count and arrangement to the
game. (Dual-sheet textured; `--tex0 tex0.png --tex1 tex1.png`.) The band and the
Akira tag are in the `o` names and in `scene_groups.csv` so pieces can be
isolated in a viewer. Orientation is view-space (camera composed in); a viewer
rotation may be needed, which Step 3's camera recovery will formalise.

## Next — Step 3

Recover the per-frame camera/view transform (the motion shared by the
camera-common band) and remove it, then classify each object by its *residual*
(camera-free) motion: static residual = scenery, moving residual = fighter.
Success vs ground truth: Akira anchor `0x130a44` -> fighter, a stage record ->
scenery; counts partition cleanly into decor / fighter-1 / fighter-2 / shadows.
