# geo_pipeline_step4e_uv_tile_wrap_v0863 — OBJ UV tile-wrap (Lau's back mosaic)

**Geometriser pipeline, Step 4 (colours), UV defect.** kazuyette, in Blender: Lau's
back has a pattern in the game, but in `lau.obj` it is a **mosaic** -- each polygon
shows a piece of texture that is not the right one, as if the tiles were shuffled. A
texture-coordinate problem, not colour.

## Diagnosis [REFERENCE-CHECKED + MEASURED]

The three candidate sites check out against MAME:
- **texheader decode** (size / rect / mirror / wrap bits): matches model2_v.cpp:
  texwidth `32<<(th0&7)`, texx `32*(th2&0x3f)`, mirror `th0>>8/9`, wrap `th0>>6/7`.
- **UV 13.3** -> `raw/8` texels.
- **texel addressing** (`vf2_texture_sheet_texel`): the 2x2 swizzle and the
  `if (x>=1024){x-=1024; y^=1024;}` fold match MAME `get_texel` exactly -- and since
  `tex_x`/`tex_y` are multiples of 32 (even), the fold preserves the nibble parity, so
  using the folded coord is equivalent to MAME using the within-texture coord.

The bug is where the UV **wrap** happens. MAME wraps **per pixel**
(`u0 = (u>>8) & (tex_width-1)`, model2rd.ipp:156), which the scene render reproduces
(`gc_fill_tri_tex` does `fmod(tu, tex_width)` per pixel after interpolating the raw
UVs -- so the scene back is correct). But `ext_emit_piece`, which bakes the OBJ, did
`fmod(tu, tex_width)` **per vertex**. A `VF2_UV_DUMP` trace of Lau found 1913 wrap
hits, concentrated in record **0x69a890** (the back): e.g. prim 2 of a 64x128 tile at
(128,640) has three vertices with texel V ~104-109 and one at exactly V=128, which the
per-vertex fmod wrapped to **0** -- scrambling the triangle's interpolation (the
mosaic). A polygon that lives entirely in a higher repeat (prim 24: all V in
[128,256)) was fine, because every vertex shifted by the same -128.

## Fix

In `ext_emit_piece`, wrap each polygon with **one common offset per axis**
(`floor(min_uv / size) * size`, subtracted from every vertex) instead of a per-vertex
fmod. This keeps the triangle contiguous and still folds a wholly-in-a-higher-repeat
polygon back onto the tile exactly as before. After: record 0x69a890 prim 2's texel V
is `104.5 108.9 128.0 126.6` (contiguous) instead of the scrambled `... 0.0 ...`;
prim 24 is unchanged (`1.6 51.2 45.8 0.0`). Polygons whose UVs stay within one tile
(the face, trousers, most of the body) are unchanged -- offset 0, fmod was already a
no-op -- so only the boundary-straddling polygons move. A baked OBJ still cannot
reproduce MAME's per-pixel repeat of a sub-rectangle, so a polygon genuinely spanning
more than one tile is approximate, but it is now contiguous (no mosaic).

Only `ext_emit_piece` (the OBJ UVs) changed; the scene render, the coloured atlas and
the texel/texheader decode are untouched.

## Validation [MEASURED]

- Scene (the correct per-pixel reference) vs screen.png over Lau's back region (the
  three-quarter view, x 325-380 / y 150-250): mean colour error **26.3/255**, in line
  with whole-Lau 27.7 -- the back's texture mapping/UV source is right, which the OBJ
  now matches. Side-by-side crop confirms the green gi + dark back pattern.
- `geo-colour` PNG byte-identical (the fix is OBJ-only); `lau.obj`/`akira.obj` and the
  coloured atlases byte-identical gcc vs clang.

gcc and clang build clean under the CI warnings-as-errors flags; geometry differential
+ display tests pass. No capture/texture/PNG/OBJ committed.

## Next

For a fully faithful repeating-texture OBJ (a polygon spanning several tiles), a
per-tile atlas with the texture tiled into its own cell would be needed; not warranted
by what the comparison shows. Remaining fidelity items stay: mip/bilinear filtering,
perspective-correct UV. `0x581d50` / `0x1a9b54` stay excluded.
