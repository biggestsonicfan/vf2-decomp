# geo_pipeline_step5f_floor_winding_v0883 — central arena slab was flipped; floor-up winding for tiled floors

**Geometriser pipeline, Step 5 (decor), OBJ defect.** kazuyette, on the single-frame
`stage_04/stage.obj` in Solid view:

1. **"Le carré central de l'arène a la couleur du fond de la vue."** The central stone
   slab (`rec a9bea0`, material `tile3`) showed the viewport background from above — a
   back-facing poly in a backface-culling viewer.
2. **"Semis de petits triangles à plat au sol derrière le mur à créneaux."** Flattened
   shadow-looking triangles on the ground.
3. **"Le mur à créneaux est ouvert de dos."** The battlement wall has no back.

## 1. Central slab flipped [FIXED, MEASURED]

`a9bea0` is the only geometrically flat object in stage 4 (Yext=0, XZ=48, 8 faces,
material `tile3`). All 8 of its faces had a model normal pointing **down** — VF2 model
normals are not consistently outward (already relied on by `ext_emit_poly`'s winding
orientation), and here the floor's own normal points into the ground, so the OBJ winding
came out downward and the viewport culled it.

Fix in `ext_emit_piece`: a **repeated-tile floor** (`poly_tiled`) whose model normal is
near-vertical but points down gets its orientation normal forced to +Y (`ext_up_normal`),
so the winding faces up. Gated on `poly_tiled`, which is stage-only (fighters pass
`tiles==NULL`), and on the normal being within ~25° of straight down
(`-pnrm[1] > 0.9*|pnrm|`), so near-horizontal **walls** and non-floor tiles are untouched.
Verified: `a9bea0` now winds +Y (geometric normal (0,+772,0)); 10 of 11 stages have
**every** flat tiled floor facing up (stage_00..10: all `down=0` except stage 3). Not
double-exported (one record, tiles path only). Fighter path byte-identical (akira).

## 2. "Flat shadow triangles" — no separate flat objects [MEASURED, no change]

Per-object flatness of stage 4: **only `a9bea0` is flat**, and it is the real arena
floor (must be kept). `piece25_rec5a03a8` (the object kazuyette named as a landmark "in
the zone") is a tall thin post (XZ=2.0, Yext=11.5), not flat triangles. So there are no
separate flat shadow *objects* to exclude — a per-object flat threshold would delete the
arena floor itself. The flat triangles kazuyette saw were the down-facing `a9bea0` slab
(fixed in §1) and/or flat floor *faces* inside multi-face decor bases (`piece08` 46
faces, `piece11` 108 faces) which are legitimate structure bases, not shadows. The
existing shadow filter (matrix-flatness `gs_flatness < eps`) already drops true shadow
objects at capture time; no geometry threshold added.

## 3. Crenellated wall open at back — ROM model, not export loss [EXPLAINED]

The geometriser decode is byte-exact (validated on the 11 fighters, same `ext_emit_piece`
path) and emits **every** polygon in the ROM record — nothing is dropped. An open back
therefore means the ROM model carries **no back-face polygons**: a one-sided wall, the
standard Model 2 approach (the in-game camera never sees the back, so back faces are
omitted to save polygon budget). Not an export loss.

## Residuals (follow-up)

- **Stage 3 floor `rec57fd08`** still winds down on its two large flat quads
  (8.5×8, 9.2×8 at Y=−0.5). Its model normal is **not** a clean near-vertical-down
  (degenerate/near-horizontal), so the normal-based flip in §1 cannot reach it.
  A robust fix needs a **geometry-based** floor rule (flip a geometrically flat,
  floor-level tiled poly to +Y regardless of its stored normal) — deferred because it
  touches the winding decision for every tiled poly and wants its own regression pass.
- **Recovered-object unbaked texels** (kazuyette: "intègre à l'atlas les zones de
  texture des objets récupérés"). Confirmed real: some ROM-recovered decor polys
  (stage-4 `rec59afe0`, `rec59b2e0`) sample atlas texels that no bake rect ever wrote
  (proven with a visited-sentinel: truly-unbaked, not translucent cutout). Root cause is
  **not** the mild-repeat tiling threshold — lowering it 1.5×→1.05× did not fix it — but
  a deeper UV/texheader divergence between `ext_emit_piece` (UVs) and
  `ext_colour_sheet_piece` (bake) on recovered records; both read the same `th_ptr` yet
  the UV lands outside the baked rect. Needs a focused session: instrument the per-poly
  `th_ptr` walk on a recovered record and compare the emit vs bake rect per prim.

## Build / export

gcc + clang clean under `-DVF2_WARNINGS_AS_ERRORS=ON`; geo/display/geometry tests pass
(5/5). All 11 single-frame `stage_NN` and from-ROM `stage_NN_full` re-exported; every MTL
passes the texture-existence check. No ROM/capture/OBJ committed (stages local in
`..\snap\stages_obj_colour\`).
