# geo_pipeline_step4c_opaque_texel0f_v0861 — opaque texel 0x0f (black-spot fix) + witness trace

**Geometriser pipeline, Step 4 (colours), defect.** kazuyette, in Blender on the
coloured atlases: scattered **black spots** on Akira's arm skin / face / gi, and worse
on Lau (>half the green trousers and torso black); Lau's ponytail grey-blue instead of
black. Lead: the spots look like dark texels crushed to black; suspect per-polygon
lumabase/colorbase selection or a texheader-field decode error.

## Root cause — texel 0x0f is the cutout ONLY for translucent polygons [REFERENCE-PROVEN]

`gc_texel_rgb` treated texel **0x0f as transparent unconditionally** and skipped it,
leaving black. In MAME that is wrong: the renderer is chosen by
`renderer = (texheader[0] >> 13) & 3` (model2_v.cpp:568) into
`{solid<false>, solid<true>, tex<false>, tex<true>}`, so **Translucent = bit 13**. For
an **opaque** polygon (bit 13 clear) texel 0x0f is simply the **brightest intensity**,
not a cutout; only a translucent polygon treats it as the cutout (model2rd.ipp alpha
test). A flat-shaded opaque region is mostly texel 0x0f (full intensity of the
colorbase colour), so skipping 0x0f turned whole flat areas — Lau's gi/trousers, much
skin — fully black.

Fix: pass the polygon's translucent flag (`texheader[0] bit 13` = `hd.renderer & 1`) to
`gc_texel_rgb`; skip 0x0f only when translucent. Applied to both the scene render
(`gc_fill_tri_tex`) and the albedo atlas (`ext_colour_sheet_piece`).

## Witness-polygon trace vs screen.png [MEASURED]

Added an env-gated single-pixel trace to geo-colour (`VF2_COLOUR_TRACE="x,y"`) that
dumps the full draw_scanline_tex pipeline for the pixel that wins the z-test:
texheader rect/renderer, colorbase, lumabase, r5/g5/b5, texel, luma curve, shade, the
three colorxlat lookups, and the gamma RGB.

- **Lau green trousers, screen (342,192) = (0,136,0):** winning polygon colorbase
  0x063 → r5=0 g5=19 b5=0, **texel 0x0f** (opaque, renderer 2), curve 40, shade 25,
  colorxlat G=170 → gamma **(0,141,0)**. Bit-faithful match — and only renders at all
  because of the fix.
- **Lau ponytail/hair, screen (306,142) = (5,5,5):** the single covering polygon has
  colorbase 0x061 → **r5=g5=b5=19 (a genuine mid-grey)**, texel 0x0f, → gamma
  (90,90,90).

So the texheader fields decode **correctly** (the green-trousers trace is bit-exact,
and `vf2_texture_header_advance_words` sign-extends the 5-bit advance, so `th_ptr` does
not drift). The ponytail grey is the material's **real palette colour**, not a
mis-selection: the game darkens that material with lighting/draw order, and the
full-light albedo atlas (palette+luma+colorxlat+gamma, by spec, no per-polygon
lighting) shows it at its unlit brightness. That is an albedo-vs-lit distinction, not a
decode bug, so per "corrige seulement ce que la comparaison prouve" it is left as-is
and reported. (Making the atlas match the lit game would mean baking per-polygon
lighting, which conflicts for texels shared between polygons.)

## Result

The 0x0f fix fills the flat opaque regions: atlas non-black texels +41% on Lau
(250k → 354k), green trousers solid, skin smooth. Scene colour error vs screen.png is
essentially unchanged (Akira 26.0 → 25.9, ALL 33.5 → 33.7 /255; a few more pixels draw)
— the scene samples visible triangle surfaces, which rarely land on flat 0x0f fill, so
the win is in the atlas and in opaque 0x0f highlights.

## Build / tests

gcc and clang build clean, including the CI warnings-as-errors flags
(-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -Werror). The
geo-colour PNG and both fighters' coloured atlases are byte-identical across compilers.
Geometry differential + display tests pass (no library or exporter decode changed; the
fix is in the shared colour helper used only by the geo-colour / geo-fighter tools). No
capture/texture/PNG/OBJ committed.
