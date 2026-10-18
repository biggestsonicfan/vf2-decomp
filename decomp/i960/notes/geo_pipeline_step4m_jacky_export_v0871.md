# geo_pipeline_step4m_jacky_export_v0871 — generalize geo-fighter for new characters (Jacky)

**Geometriser pipeline, Step 4.** kazuyette added a third character capture, `snap\colour_jacky\`
(Jacky P1 vs CPU Lau, frame 3706, same layout as colour_f, texparams from the boot `tparams\`).
Export jacky.obj/.mtl/textures and verify.

## Change

`geo-fighter` previously accepted only `akira`|`lau`. It now takes any fighter name: the
left/right side (which connected-component group to keep) defaults to left (P1), with `lau`
mapped to right (the opponent in the captures so far); an explicit `-l`/`-r` suffix on the
name overrides it. A `VF2_CASE_CHECK=1` env prints, per run, the count of candidate pieces
with a reflection matrix (det<0), the count not attached to either fighter (unassigned), and
the record of any det<0 piece in the selected fighter -- the recurring checks for a new
character.

## Validation [MEASURED] -- Jacky

- `geo-fighter ... jacky` selects 32 pieces / 2236 triangles (left), bbox 0.80x1.65x0.95.
  jacky.obj renders complete and on-model: blonde spiky hair, bare arms with white wrist
  wraps, dark-red tank, belted waist, maroon trousers, black boots -- front and back clean
  (the Step 4f-4l fixes apply: alpha cutouts, decal lift, winding, 4-gon faces, link-0 cull,
  two-sided decal luma).
- Geometry overlay on colour_jacky/screen.png: edge-align score 158.6 at dx=0 dy=-3 (the
  known ~1-frame RAM9/screen lag); both fighters' silhouettes track their sprites.
- Scene colour error vs colour_jacky/screen.png: Jacky (left) 22.3, Lau (right) 26.5, decor
  26.9, ALL 25.7 /255.
- New-case check: **det<0 = 0** (no reflection matrices), **1 unassigned** candidate (a small
  outlier not attached to either fighter; Jacky is complete so it is not a visible body part),
  transparency present and handled by the existing translucent path.

gcc and clang build clean under `-DVF2_WARNINGS_AS_ERRORS=ON`; targeted geo/display/geometry
tests pass. No capture/texture/PNG/OBJ committed (jacky.obj and the colour atlases are local
in snap/fighters_obj_colour, never committed).

## Next

Watch for a det<0 (reflection/mirror) matrix on a future character -- the normal transform
flips, and `transform_vector` + the `|dotl|` luma (Step 4l two-sided for decals) should handle
it, but it is untested. The 1 unassigned Jacky candidate is worth a look if a part turns up
missing. `0x581d50` / `0x1a9b54` stay excluded.
