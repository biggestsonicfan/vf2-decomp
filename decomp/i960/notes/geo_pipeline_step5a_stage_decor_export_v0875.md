# geo_pipeline_step5a_stage_decor_export_v0875 — export the stage decor (geo-stage) and merge across captures

**Geometriser pipeline, Step 5 (decor).** First decor step, no new capture. Export the
static stage -- every non-fighter, non-shadow object (the ones posed by the shared
decor/camera matrix) -- with the same colour atlas + decal chain as the fighters, and
merge the per-capture results by object record to cover as much of the stage as
possible.

## Tool: `geo-stage`

`geo-stage <ram9> <readstarts> <rom-dir> <tex0> <tex1> <pose> <out-dir>
[colour-capture-dir] [texparams.txt]` -- `command_geo_fighter` with a `stage_mode`
flag (the fighter path is byte-for-byte unchanged when the flag is 0). In stage mode
it keeps exactly the objects the fighter export drops: ROM records in the decor range
`0x3c1fac..0x3ca730` (deduped), skips the fighter connected-components step (every
decor object is selected), and does **not** recentre. A decor object's submission
matrix *is* the decor/camera matrix, so `gs_worldframe` maps it to identity -- the
vertices land in a camera-independent world frame that is the same in every capture,
so the same record has byte-identical geometry across captures (verified: max
per-vertex diff 0.0 between colour_f and colour_jacky) and the per-capture stages
merge/dedup cleanly by record. Writes `stage.obj` / `stage.mtl` / `stage_tex0/1.png`.

## Validation [MEASURED] -- the stone-towers VS stage

Distinct decor objects per capture: colour_f 20, colour_jacky **24**, colour_pai_lion
20, colour_sarah_shun 20, colour_wolf_kage 20, colour_jeffry_akira 20,
colour_akira_dural 20. `vf2_fight_ram9_screen_30f` (30 complete-RAM9 fight frames) is
the **same** stage -- all 30 frames show the same 20 base records, none new.

**Union = 27 distinct objects** (jacky's wider idle pose brings extra wall segments
into frustum). Merged (tools/python, by record, atlas composited over the captures)
into `..\snap\stages_obj_colour\stage.obj`:
- **Floor**: 4 large ground tiles (records 3c1fac/3c2324/3c34a4/3c381c, ~96×96 each,
  a 193×193 ground plane). Present.
- **Walls / towers / ivy**: 23 stone wall-and-tower segments with windows, with green
  ivy/bush objects between them -- the castle courtyard wall. Present, textured
  (stone + ivy), on-model vs screen.png.
- **CASE_CHECK: det<0 = 0** on every capture.

## What is missing -- and is 2D, not 3D [MEASURED]

The 3D decor is only the floor and the near castle wall (all objects within
x∈[−21,20], the arena). **The sky and the background mountains are NOT 3D** -- no
decor object reaches the horizon or the upper background. In the screenshots the blue
sky gradient and the distant mountains sit *behind* the 3D towers, in the gaps: they
are the Model2 **2D tilemap layer** (the same layer as the HUD, which the 3D-only
render never produces; `snap\vf2_fight_tileram_f1536.bin` holds it). They are not
invented here -- flagged as 2D, to be handled separately if wanted.

No ROM/capture/OBJ/atlas committed; the merged stage is local in
`..\snap\stages_obj_colour\`. gcc+clang clean under `-DVF2_WARNINGS_AS_ERRORS=ON`;
targeted geo/display/geometry tests pass.

## Next

More stages need captures of other arcade-mode arenas (the VS stage is always the
stone towers); that needs the stage-select work-RAM variable (under investigation) so
kazuyette can force a stage. The 2D sky/mountain layer is a separate decode (tilemap
RAM) if we want the full backdrop.
