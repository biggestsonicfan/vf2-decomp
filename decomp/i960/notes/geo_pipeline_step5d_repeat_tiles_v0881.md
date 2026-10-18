# geo_pipeline_step5d_repeat_tiles_v0881 — repeat-UV tiles (stage floors) as standalone REPEAT images

**Geometriser pipeline, Step 5 (decor), OBJ defect.** kazuyette, in Blender: a stage's
main floor is a big grey untextured quad (geometry present) with only small stone/moss
patches textured. Cause: the floor polys sample a REPEATED tile, and the atlas bake
holds one copy of the tile, so the wrapped UVs run off it into blank atlas.

## Diagnosis [MEASURED]

Dumping a stage-4 floor poly (rec `0x599e64`): tile `(1280,0) 256x256`, UV texels
`v[640..1280]` = **2.5 tile repetitions**. The atlas holds one 256x256 copy, so the
export's wrapped UVs span past it -> grey. (It is not a filter/ID-range miss: the quad
is present; only its texture is wrong.) Every stage has such polys (3-17 tiles each).

## Fix

In `ext_emit_piece`, a polygon whose UV span exceeds 1.5x its tile in u or v is marked
`poly_tiled`: its UVs are emitted in **tile units** (`texel / tile_size`, so repeats
show as values > 1) against a per-tile material `tileN`, instead of the atlas. The tile
rect `(sheet, x, y, w, h)` is recorded in `gf_tiles[]`. After the atlas is baked,
`command_geo_fighter` **cuts each referenced tile out of the baked atlas** (it already
holds the tile's colour+luma) into `<base>_tileN.png`, and the MTL declares a `tileN`
material pointing at it -- the OBJ importer wraps UVs outside [0,1] as REPEAT by
default, so the tile pavements correctly. Tiling is enabled only for geo-stage; the
fighter path passes NULL and is byte-identical (verified: akira unchanged).

## Validation [MEASURED]

- Stage 4 floor: the big quad now paves instead of grey. It textures as the **moss
  surround**; the stone-paved arena at the centre is a separate poly -- both match the
  MAME-exact scene render (stone centre + moss surround), confirming the quad's texture
  is the ground the game specifies, not a bug (the camera only frames the stone arena,
  which already worked as the "stone patches").
- All 11 stages re-exported with repeat tiles: 00:4, 01:3, 02:14, 03:14, 04:5, 05:7,
  06:17, 07:3, 08:11, 09:12, 10:7 tiles. Single-frame `stage_NN` and from-ROM
  `stage_NN_full` both carry the fix.

gcc+clang clean under `-DVF2_WARNINGS_AS_ERRORS=ON`; targeted geo/display/geometry tests
pass. No ROM/capture/OBJ committed; stages local in `..\snap\stages_obj_colour\`.

## Next

The per-tile luma is baked into the tile image (one representative value, fine for the
flat floor). A non-flat repeat surface would want per-poly luma as vertex colour or a
material factor, as kazuyette noted -- deferred until one turns up. kazuyette to confirm
the paved floors in Blender.
