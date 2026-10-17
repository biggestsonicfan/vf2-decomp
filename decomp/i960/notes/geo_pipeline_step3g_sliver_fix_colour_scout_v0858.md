# geo_pipeline_step3g_sliver_fix_colour_scout_v0858 — sliver fix + colour scouting

**Geometriser pipeline, Step 3 (polish) + colour recon.** kazuyette: akira.obj is
fully recognisable and lau.obj is clean from all angles -- goal met. Two clean-ups
before colour: a few sliver triangles on Akira (hair, back, front leg), absent from
Lau; and scout where 3D polygon colour comes from.

## Sliver-triangle fix [REFERENCE-PROVEN]

Trace (`VF2_PIECE_DBG`) + reading the decoder against MAME geo_parse_np_s and sm2-emu:
for a **triangle** polygon (attr&1==0) each 10-word block is
[attr][normal][P0(n)][unused]. MAME reads P0(n) and **skips** the last slot
(geo_parse_np_s input+=3); sm2-emu sets **P1(n) = P0(n)** for triangles
(geometrizer.cpp:557). `ext_emit_piece` instead read that skipped slot as a real
vertex `vB` and used it as the triangle's 3rd corner (and in the strip link) -- a
garbage vertex, i.e. a sliver, one per triangle polygon.

Fix: for a triangle, set `vB = vA` (P1(n)=P0(n)) instead of reading the skipped
slot; the 10-word stride is unchanged. Quads are untouched.

Result: **lau.obj byte-identical** (all its polygons are quads -- why it had no
slivers), the projection PNG byte-identical, Akira's triangle polygons now emit a
correct triangle (no sliver). The synthetic geometry tests still pass.

## Corrections to earlier notes

- The **0x00040401 truncation** (v0857) was in `ext_emit_piece`, shared by every
  exporter, so **all previous extractions** (export-pieces, export-textured,
  geo-scene, pieces/*.obj, scene.obj and the earlier akira/lau exports) were
  truncated to their first 0x00040401 polygon. Only exports from v0857 on are full.
- **`0x130a44` is Lau's face, not Akira's.** v0834/v0836/v0837/v0838/v0839 guessed
  "Akira anchor 0x130a44"; corrected since v0841 (projects right = Lau) and in
  lau.obj since v0853. A correction banner was added to v0838.

## Colour recon -- where 3D polygon colour comes from [MAME]

A textured 3D pixel (MAME `draw_scanline_tex`, inputs set in model2_v.cpp:576-578;
the body is in model2rd.ipp, not in our ref copy) combines:

- **texel** -- from TEXTURE RAM, sheet0 0x12000000 / sheet1 0x12400000 (a 4-bit
  luma/intensity, not an RGB index). HAVE: `vf2_fight_tex0/1_f1808.bin`.
- **lumabase** = (texheader[1] & 0xff) << 7, **colorbase** = (texheader[3] >> 6) &
  0x3ff -- per polygon, from the texture header. HAVE (th_ptr in the capture).
- **polygon luma** -- lighting (normal . light), computed in geo_parse; no RAM.
- **luma RAM** (`m_lumaram`) 0x12800000-0x1281ffff (0x20000 addr space, 1 byte per
  32-bit word): maps luma/texel to intensity. HAVE? `vf2_fight_luma_f1808.bin` is
  0x20000 -- matches this region (please confirm the capture address).
- **palette** 0x01800000-0x01803fff (0x4000, RGB15) then **colorxlat**
  0x01810000-0x0181bfff (0xc000, the per-channel gamma that `palette_w`
  model2.cpp:363-369 applies) -> final RGB. `vf2_fight_palette_f1536.bin` is 0x4000
  -- matches the 0x01800000 palette (it is the SHARED 2D/3D palette, not tile-only).

**What to capture for colour:** we likely already have the texture sheets, the
palette (0x01800000), and the luma RAM (0x12800000). The one region not yet captured
is **colorxlat 0x01810000-0x0181bfff (0xc000 bytes)** -- needed to turn the palette's
RGB15 into the exact on-screen RGB (a plain RGB15->RGB24 scale is a usable first
approximation without it). Please also confirm the two existing captures are of
0x01800000 and 0x12800000 at a FIGHT frame (not a menu).

## Next

Colour: wire texel + texheader colorbase/lumabase + luma RAM -> palette -> RGB into
the OBJ/MTL export (vertex colour or an RGB texture), once colorxlat is captured /
confirmed. `0x581d50` / `0x1a9b54` stay excluded.

gcc and clang build clean and produce byte-identical OBJs (lau.obj unchanged) + a
byte-identical projection PNG; vf2_geo_display + the geometry differential tests
pass. No capture/texture/OBJ data committed.
