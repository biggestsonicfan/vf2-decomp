# geo_pipeline_step4l_twosided_decal_luma_v0870 — two-sided luma for translucent decals (Lau's front motif)

**Geometriser pipeline, Step 4 (colours), OBJ defect.** kazuyette: on Lau's front, the green
motif's left half is green but the right half (the same motif, mirrored) comes out near-black,
with a black corner at the centre. The geometry is fine in Solid view -- it is a colour
problem on the mirrored decal polys.

## Diagnosis [REFERENCE-CHECKED + MEASURED]

Dumping the luma + normal of the front ornament polys (record 0x69a890, tile (384,0 128x256),
renderer=3 translucent, single-sided): the left half has `dotl*dotp >= 0` (lit, lum up to
207); the mirrored right half has `dotl*dotp < 0`, so `gc_poly_luma` returns ambient only
(lum=63) -- the two halves share one record, so mirroring flips the normal and the light then
reaches only one side.

This is **not** a bug in `gc_poly_luma`: it matches MAME's `geo_parse_np_s` luminance exactly
(model2_v.cpp:1240-1253: `dotl*dotp<0 ? 0 : fabs(dotl)`, then `*diffuse + ambient + specular`;
never negative, `fabs` is used). It is **not** from the winding fix (Step 4i, which only
reorders OBJ corners, not the normal the luma reads). The difference from the game: MAME draws
the translucent decal **over the lit red base** (checker blend), so the shadowed half still
reads lit; a baked opaque atlas instead shows it near-black.

## Fix

`gc_poly_luma` gains a `twosided` flag; when set it uses `fabs(dotl)` regardless of the
`dotl*dotp` sign. The atlas bake (`ext_colour_sheet_piece`) passes `twosided = translucent`,
so a translucent decal is lit on both faces and its mirror halves match -- correct for a
static OBJ viewed from any angle. Opaque polys keep single-sided lighting (so hair/shoes stay
dark, Step 4d). The scene render computes its luma inline and is untouched, so it stays
MAME-exact.

## Validation [MEASURED]

- Lau's front green motif is now fully green and left/right symmetric; the back dragon, the
  green trouser cuffs, hair, shoes and Akira are unchanged. With two-sided lighting the
  formerly-shadowed ornament polys go from lum=63 to 137-225 (matching the lit half).
- Scene colour error vs colour_f/screen.png is byte-unchanged (atlas-only change): Akira 25.7,
  Lau 24.4, decor 38.7, ALL 32.8. akira.obj / lau.obj re-exported.

gcc and clang build clean under `-DVF2_WARNINGS_AS_ERRORS=ON`; targeted geo/display/geometry
tests pass. No capture/texture/PNG/OBJ committed.

## Next

A fully faithful static bake of a translucent decal would blend it with the lit base colour
rather than light it two-sided; two-sided is the simpler approximation that reads correctly.
`0x581d50` / `0x1a9b54` stay excluded.
