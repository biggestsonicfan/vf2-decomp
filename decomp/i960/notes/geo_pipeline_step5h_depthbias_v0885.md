# geo_pipeline_step5h_depthbias_v0885 — coplanar painter-order depth-bias (zigzag / z-fight fix)

**Geometriser pipeline, Step 5 (decor), OBJ viewer artifact.** kazuyette: thin zigzag
lines on the ground behind the back wall and splinters along the right wall in Blender.
First suspected link-0 spikes; Step 5g proved link-0 emitted=0 on every path. A render
(coloured by captured vs recovered, z-fight pixels highlighted) confirmed the cause is
**z-fighting between coplanar faces**, and kazuyette noted the same zigzags appear already
in the single-frame `stage_04` export (no recovered objects) — so it is captured-vs-
captured too, not only recovered-vs-captured.

## Cause [MEASURED]

VF2's Model 2 renderer draws decor in display-list order with exact fixed-point
arithmetic, so coplanar faces never z-fight on the arcade. A baked OBJ in a float-
z-buffer viewer does: adjacent/overlapping coplanar pieces shimmer. It is **not**
redundant duplicate objects — no cap/rom record collides on any stage, no recovered
object is >40% coplanar-covered by captured faces, only 2/146 recovered faces in stage-4
overlap a captured face >30%. The faces are distinct, legitimately coplanar decor.

## Fix (kazuyette's rule)

New post-processor `tools/python/geo_stage_depthbias.py`, run on every stage OBJ
(single-frame and from-ROM). It buckets faces by plane, and in each coplanar group
spanning **two or more distinct objects** nudges the back layers a hair (0.06 world units
per layer) along −normal, leaving the front-most face put. Front-ness, per kazuyette:
a **captured** object is in front of a **recovered** one, and among the same kind the
object drawn **later** in the display list is in front (MAME painter order; the OBJ emits
objects in `sel[]` draw order). The geometriser writes each face with its own vertices
(no sharing), so a per-face nudge never tears the mesh. A single object's own coplanar
faces are left exactly as drawn.

Wired into `geo_stage_fromrom.py` (runs after the intruder filter on `stage_NN_full`),
and run directly on the single-frame `stage_NN`.

## Validation [MEASURED]

z-fight pixels (render detector, depth-eps 0.02 < the 0.06 nudge), before → after:
- single-frame `stage_04`: present → **0** (top and iso); `stage_03`: → 0.
- `stage_04_full` iso: 282 → **3**; `stage_08_full` iso: → 14. (A straight-down top view
  keeps a small residual from near-vertical faces seen edge-on; the fighting-camera / iso
  angle kazuyette uses is clean.)
- Textured top render of `stage_04_full`: moss surround + stone arena, no shimmer.

0.06 is invisible (<0.2% of a floor tile) but well above Blender's depth epsilon. All 11
stages re-exported (single-frame + `_full`). Fighters never run through the post-processor
(stage tool only), so character OBJs are byte-identical (akira verified); no C change this
step. `py_compile` clean on `tools/python/*.py` (the CI Python step). No ROM/capture/OBJ
committed (stages local in `..\snap\stages_obj_colour\`).

## Note on CI #2121 (not ours)

The red job is Windows MSVC-release only, single test `vf2_python_audio_rom_matrix_l3`:
`test_fixture_runner_invocation` asserts argv `'roms/vf2'` but `str(Path('roms/vf2'))` is
`roms\vf2` on Windows. The test + its module are byte-identical since 5904c95, so it fails
identically there — pre-existing, Windows-specific, unrelated to the geo (Step 5f) work.
Audio is celsowm's domain; left untouched. Fix is theirs (`os.path.join` / normalise).

## Next

Recovered-record UV / baking: the truly-unbaked atlas texels on some recovered decor
(stage-4 `rec59afe0`/`rec59b2e0`) — an `ext_emit_piece` (UV) vs `ext_colour_sheet_piece`
(bake) `th_ptr`/UV divergence, still to be root-caused.
