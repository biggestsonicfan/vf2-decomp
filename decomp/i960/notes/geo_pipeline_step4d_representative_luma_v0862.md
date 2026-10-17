# geo_pipeline_step4d_representative_luma_v0862 — bake the atlas with real per-polygon luma (not full light)

**Geometriser pipeline, Step 4 (colours), defect.** kazuyette, in Blender: dark
materials come out too light. First the solid-colour polygons (ponytail, top of head,
shoes grey-white vs black); then, looking closer, the hair (textured, strands visible)
sits on a grey background instead of black. The cause is general: the atlas was baked
at **full light** (lumascale 0xff), but in-game the colour of every polygon depends on
its **luma** -- colorxlat is indexed by the shade, which is the lighting term -- so a
dark material is dark because its luma is low, not because its texels are black.
"Full light" is meaningless for shading; bake with a **representative luma** instead.

## Fixes

### Untextured (solid) polygons [REFERENCE-PROVEN]
MAME `draw_scanline_solid` (model2rd.ipp:46-99): for an untextured polygon there is no
texel -- `luma = object.luma >> 2` indexes colorxlat directly (same colorbase +0x1000
indirection), and a **translucent solid renders nothing**. Renderer is
`(texheader[0]>>13)&3`: bit1 textured, bit0 translucent. The scene render
(`gc_fill_tri_tex`) and the atlas now take the untextured path: shade = luma>>2, one
colorxlat colour, no texel. (Few fighter polygons are untextured, so the scene number
barely moves -- the win is in the atlas and the general luma fix below.)

### The whole atlas, textured and solid, with real per-polygon luma
`ext_colour_sheet_piece` no longer bakes at full light. It reads each polygon's normal
and first vertex and computes the **real per-polygon luma** at the colour_f frame via
the new shared `gc_poly_luma` (normal x matrix, dotl, dotp front/back, diffuse +
ambient + specular from the persisted 0x06 table) -- exactly the term the scene render
uses. `geo-fighter` gained the plumbing: it stores each candidate's view-space matrix
(`vf2_geo_object.matrix`, before camera removal), recovers the frame's light vector
(0x0a) and geo mode (0x07), and loads the texparams from an optional final arg:

  `geo-fighter <ram9> <readstarts> <rom> <tex0> <tex1> <pose> <which> <out> [colour-dir] [texparams.txt]`

With the texparams file the atlas uses real luma; without it (or without the colour
dir) it falls back to the old full-light albedo. Textured polygons colour each texel
with this luma; untextured ones fill their rectangle with the one solid colour.

**Why representative, not average:** VF2's light vector has magnitude 1.30, so
well-lit skin/green saturate the luma near 255 -- a uniform "ambient + average diffuse"
would wrongly darken those. The real per-polygon luma keeps lit materials bright and
darkens only the shadowed/dark ones.

## Result [MEASURED]

Lau atlas (`lau_tex1.png`), hair region mean brightness: **97.8/255 full light ->
33.4/255 real luma** (3x darker -- the hair is now dark, as in the game), while the
face stays skin-toned and the well-lit green trousers stay bright (the green-trousers
polygon renders (0,141,0) vs screen (0,136,0)). The atlas now bakes the identical
per-polygon colour the scene render produces, so the atlas-textured model matches the
scene, which is Akira 25.9 / Lau 27.7 / all 33.7 /255 vs screen.png (unchanged:
geo-colour's texel path is byte-identical). Open the OBJ in a standard OBJ viewer; the
built-in `vf2_model_viewer` stays grayscale raw-bank.

## Build / tests

gcc and clang build clean, including the CI warnings-as-errors flags
(`-DVF2_WARNINGS_AS_ERRORS=ON`). The geo-colour PNG (byte-identical to v0861's) and
both fighters' OBJ + coloured atlas are byte-identical across compilers. Geometry
differential + display tests pass (no library or exporter geometry decode changed). No
capture/texture/PNG/OBJ committed.

## Next

A single grey scene pixel on Lau's hair (306,142) still reads (90,90,90) vs screen
(5,5,5): that polygon's computed luma (103) is moderate while the game shows it black,
a localised residual consistent with draw order (painter's z-sort vs our z-buffer) or
a lighting-magnitude detail, not a decode bug. Remaining fidelity items stay: mip/
bilinear filtering, perspective-correct UV. `0x581d50` / `0x1a9b54` stay excluded.
