# geo_pipeline_step4f_translucent_alpha_atlas_v0864 — RGBA atlas, cutout texels transparent (eyes)

**Geometriser pipeline, Step 4 (colours), OBJ defect.** kazuyette, in Blender on
`akira.obj` (and Lau): a continuous black band covers both eyes temple-to-temple, like a
mask; in the game you see the eyes, eyebrows and skin around them.

## Diagnosis [REFERENCE-CHECKED + MEASURED]

The eye band is a **translucent decal** polygon (renderer `(texheader[0]>>13)&3` has bit0
set; `checker = (texheader[0]>>15)&1` is 0, so MAME uses the binary alpha-test path, not the
screen-door stipple). Its tile (sheet1 (192,576) 64x64) is **37% texel 0x0f** (the cutout) +
63% low texels; its colorbase (0x031) is a grey->white ramp — the eye whites/highlights.

- In the **scene** render (`gc_fill_tri_tex`) a translucent polygon's 0x0f texels are
  skipped (`gc_texel_rgb` returns 0), so the well-lit skin polygon behind shows through and
  the eyes read correctly.
- In the **baked OBJ** the colour atlas was RGB (no alpha) and the MTL had only `map_Kd`, so
  those cutout texels baked as opaque black and the decal occluded the skin — a solid black
  band. Same class of loss as the UV "mosaic": an OBJ can't reproduce the scene's per-pixel
  cutout at draw time, so it must bake the transparency.

This matches MAME exactly: `draw_scanline_tex<Translucent>` discards texels failing the
alpha test (model2rd.ipp:326-334); we discard texel 0x0f for a translucent polygon, which is
the VF2 cutout. Nothing in the texel->colour conversion is wrong.

## Fix

`ext_colour_sheet_piece` now also writes a per-sheet **alpha plane**: a painted texel
(opaque, or a kept translucent texel) gets alpha 0xff; a translucent polygon's cutout (0x0f,
where `gc_texel_rgb` returns 0) gets alpha 0; the plane starts at 0 (unpainted =
transparent). A solid (untextured) polygon fills alpha 0xff. The coloured fighter atlas is
written with the new `png_write_rgba8` (colour type 6) instead of `png_write_rgb8`, and the
per-fighter MTL emits `illum 1`, `d 1.0`, and `map_d <sheet>.png` alongside `map_Kd` so an
OBJ viewer uses the alpha. The scene render, the texel/colorbase/luma decode and the
grayscale export are untouched; only the coloured OBJ atlas path changed.

### Blender setup
The RGBA PNG carries the alpha in `map_Kd`; `map_d` points at the same file. On import,
set the material **Blend Mode = Alpha Clip** (and Shadow Mode = Alpha Clip), Clip Threshold
~0.5, and enable **Show Backface = off / Backface Culling** as desired. Alpha Clip (not Alpha
Blend) is correct here: the cutout is 1-bit, and clipping lets the skin polygon behind show
without depth-sort artefacts. (If the importer already wired the PNG alpha to Base Color
Alpha, only the Blend Mode change is needed.)

## Validation [MEASURED]

Akira's face, baked OBJ rendered with alpha-clip (skin polygon behind shown through the
cutouts): the black mask is gone; both eyes, eyebrows and the eye-white detail are visible on
clean skin (`akira_face_alpha.png` vs the former `akira_face.png`). Atlas PNGs are now
colortype 6 (RGBA) 2048x1024. gcc and clang build clean under the CI warnings-as-errors
flags (`-DVF2_WARNINGS_AS_ERRORS=ON`); geometry differential + display tests pass. No
capture/texture/PNG/OBJ committed.

## Next

**Texheader source bug (the real vest fix), found while tracing the vest and confirmed
against MAME `geo_object_data` (model2_v.cpp:1715-1728).** The 0x01 object command carries
`tpa`=w[0] (texture points), **`tha`=w[1] (texture header)**, `oba`=w[2], `obc`=w[3]; MAME
reads the texheader straight from `tha`. We instead derive `th_ptr` from a `tpa`->id->table
`[0x020e0004 + id*16 + 4]` lookup inherited from the early geometry steps. The lookup returns
the first table entry whose w0==tpa, whose w1 can differ from this command's actual `tha`:
**38/113 objects this frame are DIFF, 7 with a different colorbase**, including Lau's vest
**0x69a890** — our lookup gives colorbase 0x01e (colorxlat sub-29 = navy) but the direct
`tha` gives **0x020** (sub-31 = red). So the vest should read as red; the navy is a
wrong-texheader read, not a colour-conversion bug (conversion is byte-faithful to MAME:
colorbase model2_v.cpp:577, luminance 1093-1099, draw_scanline 337-351). Fix = read texheader
(and UV points) directly from the command's `tha`/`tpa`, with the RAM(`&0x800000`)/ROM
selection MAME uses (model2_v.cpp:405-408). This changes tiles for ~1/3 of objects, so it is
its own step with full geometry/UV re-validation. `0x581d50` / `0x1a9b54` stay excluded.
