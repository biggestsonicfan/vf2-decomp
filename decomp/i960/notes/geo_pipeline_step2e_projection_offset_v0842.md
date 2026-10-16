# geo_pipeline_step2e_projection_offset_v0842 — projection offset fix + LS fit

**Geometriser pipeline, Step 2e.** kazuyette on the f20 overlay: fighter
proportions now credible (shadows included), but everything shifted ~+85px x /
-85px y -- shadows at waist instead of under the feet, head-to-shadow height
right. A constant shift at correct size = projection centre/offsets, not
geometry. kazuyette flagged 85 ~ the default xoff (84).

## Cause — CRTC y-offset was wrong (crop subtracted that does not exist)

Checked MAME. The vanishing-point centre is not per-object here: all 7 window
(0x03) commands carry centre (248,319) for every eye mode, and every object's
`center_sel = (opcode>>29)&3` is 0, so a wrong centre is ruled out.

MAME's `model2_3d_project` (model2_v.cpp:657) is
`x = m_crtc_xoffset + center.x + X/Z`,
`y = (384 - center.y) + m_crtc_yoffset - Y/Z`,
with `m_crtc_xoffset = 84 + hsync`, `m_crtc_yoffset = 130 + vsync`
(horizontal/vertical_sync_w). geo-project had used `xoff=76, yoff=40` -- i.e. it
subtracted an (8,90) "visible crop" that does **not** apply (the screenshot is
the rendered frame at the CRTC offsets). **Fix: xoff=84, yoff=130** (the plain
CRTC offsets; hsync/vsync ~ 0 since the decor registers at 84).

## Least-squares fit from shadow/feet landmarks [MEASURED]

`tools/python/geo_overlay.py fit` extracts each fighter's shadow centroid in the
projection (colour 40,40,52) and in the screenshot (dark lower-floor pixels),
left/right, over f00/f10/f20/f29, and fits a 2D translation, then
translation+scale.

- **At the old xoff=76/yoff=40:** the y shift was large and very consistent
  (+89..+103, mean +97); translation-only RMS 45px; translation+scale RMS 23px.
  The +97 ~ (130 - 40), pinning the y-offset bug.
- **At the corrected xoff=84/yoff=130:** the y shift collapses to **dy=+7**
  (shadows now sit under the feet). Translation-only dx=-9 dy=+7, **RMS still
  45.6px**; translation+scale scale=1.54, **RMS 23.1px**.

So a single translation does **not** fully suffice. The y-offset was a genuine
projection-formula error and is fixed. What remains is **horizontal**: the two
fighters project too close together -- shadow separation 138px vs 225px on screen
(~1.5x). The decor (same projection, same centre/focal) registers correctly, so
this residual is **fighter-specific**: the fighter world X positions (matrix
translations) / depth are compressed toward centre, not a global offset. This
matches the earlier fighter-only lead (matrix column norms 0.237..1.357 vs decor's
exact 1.000).

## Result

Corrected `overlay_f20.png`: decor edges align with the pagodas/floor, the
shadows sit under each fighter's feet, Akira's (blue) geometry overlaps his
sprite, Lau's (orange) overlaps his with some horizontal overflow (the remaining
~1.5x fighter spread). geo-project defaults are now xoff=84 yoff=130; W/H/xoff/
yoff stay overridable for tuning.

## Next

Chase the fighter horizontal compression: it is not the projection (decor is
right) but the fighter objects' world placement -- compare a decor vs fighter
object's matrix translation magnitude and depth Z, and the raw-vertex scale vs
matrix column norm, to find why the two fighters sit ~1.5x too close. Then
fighter/scenery classification + export.

gcc and clang produce a byte-identical projection PNG; the synthetic walker test
still passes. No capture/screenshot/projection data committed.
