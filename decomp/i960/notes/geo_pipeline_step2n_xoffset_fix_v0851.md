# geo_pipeline_step2n_xoffset_fix_v0851 — the offset was a global xoff (84 -> 0)

**Geometriser pipeline, Step 2n.** kazuyette's clue: the 7 windows' display area is
x 0-496 / y 127-511 -- in y it already carries the ~130 offset, but in x it starts
at **0, not 84**. The fighters needed -85px, i.e. they register with xoff ~= 0. The
84 from Step 2e had been calibrated on approximate landmarks. So: measure the DECOR
offset with the same rigour (contour correlation, since the motion mask cannot work
for the static decor), at xoff=84 and xoff=0, on f20/f29; check where MAME's
m_crtc_xoffset comes from and whether the screenshot is horizontally cropped; and if
decor and fighters both register at one xoff, fix the formula.

## 1. Decor wants xoff=0, exactly like the fighters [MEASURED]

New `geo_overlay.py fitdecor <shot> <tris>`: rasterises the decor triangle edges to
a wireframe, thresholds the screenshot Sobel to a binary edge map (dilated), and
finds the (dx,dy) maximising edge coverage, with dy constrained (decor y is right at
yoff=130).

| | xoff=84 | xoff=0 |
|---|---|---|
| decor f20 | dx=**-84**, dy=-1 | dx=**+8**, dy=+30 |
| decor f29 | dx=**-83**, dy=-2 | dx=**-1**, dy=-2 |

At xoff=84 the decor sits ~84px too far right; at xoff=0 it registers (dx ~ 0). This
is the **same** as the fighters (Step 2m: -85 at xoff=84 -> ~0 at xoff=0; re-checked
here, Akira dx=-5/-3 at xoff=0). So **both the decor and the fighters register at
xoff=0** -- the ~85px was never fighter-specific; the whole scene was shifted right
by the wrong xoff. (My Step 2h/2m "fighter-only offset" was wrong because I had only
eyeballed the decor, never contour-fit it.)

## 2. Where the 84 comes from, and no crop removes it [VERIFIED in MAME]

- `m_crtc_xoffset = 84 + (int16_t)data` (model2_v.cpp:643), set from the tilemap
  chip's horizontal-sync-out callback (`xhout_write_callback`, model2.cpp:2513) --
  i.e. from the game's CRTC programming, not a plain i960 register. The 84 (and the
  130 for y) is a MAME **per-game base** (comment model2_v.cpp:69 "a small offset
  needs to be added ... for certain games").
- The 3D polygons are placed by `model2_3d_project` at `x = m_crtc_xoffset +
  center[0] + X/Z` (:661) into the renderer map, which is copied to the screen by
  `copybitmap_trans(bitmap, destmap, 0,0, 0,0, ...)` (:738) -- **no horizontal
  crop**. The `xoffs = -m_crtc_xoffset` at :748 is the **2D framebuffer** test-mode
  path (`draw_framebuffer`), not the polygon path, so it does not apply here.
- So the screenshot carries the polygons at `+ m_crtc_xoffset`. For VF2 the measured
  effective value is **0**, so VF2's CRTC H output is ~ -84, cancelling MAME's 84
  base. y stays 130 (decor dy ~ 0 there).

## 3. Fix

`geo-project` default xoff **84 -> 0** (the effective VF2 m_crtc_xoffset; cause:
84 MAME base + VF2 hsync ~ -84). y unchanged at 130. The overlay at xoff=0 now
registers across the board: the decor edges sit on the pagodas/pillars, Akira's
geometry overlaps Akira, Lau's overlaps Lau, and the shadows sit under the feet.
This supersedes Step 2e (xoff=84) and resolves the long fighter-offset thread: it
was a **global projection x-offset error, not a fighter-matrix problem**.

Caveat: the `fitfighters` motion-mask matcher has local optima (f20 Lau returned a
spurious -97 at xoff=0; f29 Lau -13); the reliable evidence is the decor
edge-coverage fit (clean -84 -> 0) and the visual overlay.

## Next

With the projection correct, re-run the fighter/scenery separation on a clean
footing: the decor (one matrix, Tx=2.794), the fighters (per-limb matrices) and the
shadows now all project onto the screenshot, so classification + export can proceed.
0x581d50 / 0x1a9b54 stay excluded.

gcc and clang produce a byte-identical projection PNG + CSV + tris at the new
default; vf2_geo_display + the geometry differential tests pass; geo_overlay.py
parses. No capture/screenshot/overlay data committed.
