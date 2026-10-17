# geo_pipeline_step2m_offset_measure_windows_v0850 — reliable +85px, no window cause

**Geometriser pipeline, Step 2m.** For the constant ~+90px fighter offset, kazuyette
asked for (1) a reliable per-frame, per-fighter (dx,dy) by brute-force aligning the
projected fighter silhouette (objects attached by record) to the screenshot; (2) a
full dump of all fields of the 7 window (0x03) commands, which window is active per
decor/shadow/body object, and a comparison with what MAME's geo_window_data /
model2_3d_project actually use (origin, clip, offset, eye select), plus the object
opcode / mode high bits that might pick another window; and (3) if one translation
fixes it, the DATA that explains it -- not a hand fix without an identified cause.

## 1. Reliable measurement -- a real ~+85px x offset [MEASURED]

New `geo_overlay.py fitfighters <shotdir> <frame> <tris> <csv> <ref_f00.csv>`: it
bins the frame's fighter triangles to a fighter by RECORD (frozen from f00, as in
2l), fills each fighter's silhouette, builds a motion foreground mask from the
screenshot and its neighbours (isolating the moving fighters from the static stage),
and brute-forces the (dx,dy) that maximises the silhouette's overlap with that mask.

| frame | Akira dx,dy | Lau dx,dy |
|---|---|---|
| f00 | -88, -37 | -44, +39 |
| f10 | -53, -60 | -96, -15 |
| **f20** | **-89, -10** | **-82, -13** |
| **f29** | **-87, -5** | **-83, -36** |

Once the round-start zoom settles (the decor matrix Tz stops changing at ~f18), the
two fighters agree cleanly: **dx ~= -85px for BOTH**, dy small. So the offset is
**real** (not the earlier eyeball/shadow-detector artifact), it is **horizontal**,
and it is the **same for both fighters** -- a global fighter shift, not a per-fighter
spread. Early frames (f00/f10) are noisy: during the zoom the fit is unstable and
the motion mask is large. A single ~-85px x translation approximately corrects both
fighters on the settled frames; dy has no consistent nonzero value.

## 2. Windows -- every projection field is identical to the decor [MEASURED]

All 7 window(0x03) commands, every frame (f00/f10/f20/f29), decoded in full
(each of the 6 words = x=(w>>16)&0xfff, y=w&0xfff, signed 12-bit):

- viewport start **(0,127)**, viewport end **(496,511)** -- identical in all 7.
- all four vanishing points / centers (eye 0..3) = **(248,319)** -- identical.

Active window per group: decor -> win0, shadow -> win5, fighter -> win6 -- but all
seven windows carry the **same** viewport and the **same** (248,319) centers, so the
window index is irrelevant. `center_sel = (opcode>>29)&3` is **0** for all 113
objects; the focal (0x09) is **(600,600)** for every group (one 0x09 per frame);
`mode&3` is 1 for decor, 2 for shadows, 1/3 for fighters -- and mode only selects
the vertex parse format (np/nn), not the projection.

Against MAME: `model2_3d_project` (model2_v.cpp:656) places a vertex with ONLY
`x = m_crtc_xoffset + center[sel][0] + X/Z`, `y = (384-center[sel][1]) +
m_crtc_yoffset - Y/Z`. The viewport (window words 0-1) is used only for clip planes
(geo_window_data model2_v.cpp:1826, and the clip setup at :838-896) and the raster
clip rectangle -- **it never shifts a vertex**. `center_sel` comes from the object
opcode bits (`(input>>6)&3` of `opcode>>23` = `(opcode>>29)&3`, :969). So the only
projection inputs are center, crtc offset and focus -- and **all three are identical
for decor and fighters**. There is **no window/opcode/mode data that differs between
decor and fighters**, so nothing on the projection side can produce a fighter-only
offset.

## 3. One translation fits, but its cause is NOT in the projection

A single ~-85px horizontal translation fits both fighters (task 1), but task 2 shows
**no projection-side datum explains it** (identical center/viewport/focal/center_sel/
mode vs the decor, which registers correctly). So, per kazuyette, it is **not
applied by hand**. ~85px at the fighters' Z~4.6 with focus 600 is ~0.65 world-units
of X: the fighters' world placement is offset ~0.65 in X relative to the decor's
frame. That offset lives in the fighter **matrix translation or vertex origin**, not
the projection -- i.e. in how the i960/TGP builds the fighter world matrices vs the
single decor matrix (Tx=2.794). The projection is now fully ruled out.

## Next

Pin the ~0.65-unit world-X source in the matrix-build path: compare the decor
matrix frame (one matrix, Tx=2.794, large vertices) with the fighter frame (per-limb
matrices, Tx~0, tiny vertices) and find what fixed X term the fighters are missing
(or the decor carries) -- e.g. a world/ring origin the decor bakes into its matrix
that the fighters express differently. 0x581d50 / 0x1a9b54 stay excluded.

No C changed this step (geo_overlay.py fitfighters mode only); gcc and clang build
clean, vf2_geo_display + the geometry differential tests pass; geo_overlay.py
parses. No capture/screenshot/overlay data committed.
