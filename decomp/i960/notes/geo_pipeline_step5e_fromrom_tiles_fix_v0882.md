# geo_pipeline_step5e_fromrom_tiles_fix_v0882 — --from-rom must carry the tile PNGs; MTL file-existence check

**Geometriser pipeline, Step 5 (decor), export bug.** kazuyette, in Blender:
`stage_NN_full` repeat-texture floors/walls came out magenta ("missing image"). The
`_full` folders' `stage.mtl` referenced `stage_tile0..N.png` but **no tile PNGs were
present** -- only the single-frame `stage_NN` folders had them.

## Fix

`tools/python/geo_stage_fromrom.py` (the intruder filter that writes `_full`) copied the
MTL and the two atlas sheets but **not** the standalone repeat-tile images. Fixed to
also copy every `stage_tile*.png`. All 11 `_full` re-exported, tiles now present.

Added a **texture-existence check at the end of every export**, as requested:
- in C (`command_geo_fighter`, geo-stage/geo-fighter): after writing the MTL, each
  `map_Kd` / `map_d` it names must resolve to a file in the out-dir, else the command
  prints the missing reference(s) and exits non-zero.
- in the Python filter (`verify_mtl`): same check on the `_full` output; exits non-zero
  and lists the missing references.

11/11 `_full` re-exported and pass the check (tile counts 00:4 01:3 02:15 03:15 04:5
05:7 06:17 07:4 08:16 09:13 10:8). gcc+clang clean under `-DVF2_WARNINGS_AS_ERRORS=ON`;
fighter path byte-identical (akira verified).

## Central stone paving (stage 4) [MEASURED]

Checked. The central arena platform (`rec a9bea0`, material `tile3`) **is** textured --
`tile3` is the grey cobbled stone (avg luma 85, in the scene render's range), not grey
from a missing image. But a top-down render shows **black (untextured) polys remaining
in the arena centre** around the stone platform: they reference an atlas region the bake
did not cover (their texels read as transparent/empty, so they draw through). This is a
distinct defect from the magenta one -- likely ROM-recovered floor polys whose
model-table texheader points outside what the captured polys baked -- and is left as the
next item; the repeat-tile and tile-PNG fixes here are independent of it.

## Next

Investigate the unbaked central-floor polys (recovered-object texheader vs the atlas the
capture bakes); and the deferred per-poly luma as vertex colour for non-flat repeat
surfaces.
