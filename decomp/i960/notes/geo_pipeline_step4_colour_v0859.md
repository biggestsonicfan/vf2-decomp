# geo_pipeline_step4_colour_v0859 — colour pipeline (texel→luma→palette→colorxlat→RGB)

**Geometriser pipeline, Step 4 (colours).** kazuyette supplied a one-frame colour
capture (`snap/colour_f`, fight frame 3002, Akira vs Lau in guard): `ram9.bin`,
`palette_01800000.bin` (16K), `colorxlat_01810000.bin` (48K), `luma_12800000.bin`
(128K), `tex0/1_12000000.bin` (2M each), `screen.png`. Task: wire texel + colorbase/
lumabase → luma RAM → palette → colorxlat → RGB into the render, exactly like MAME
`draw_scanline_tex`, project the scene and compare the mean colour error to the
screenshot on Akira, Lau and the decor.

## New command `geo-colour` [REFERENCE-PROVEN]

`vf2i960 geo-colour <ram9.bin> <readstarts.txt> <rom-dir> <colour-capture-dir>
<pose-frame> <out.png>` (local-only; the walker/decoder it shares is covered
synthetically in CI). It walks the frame tracking matrix/focal/light/texparams/
window, decodes each ROM object's polygons (the validated `ext_emit_piece` path,
reading normals), computes per-polygon lighting, and rasterises every triangle with
a per-pixel textured + lit fill, then writes a PNG. The exact pixel formula is MAME
`model2rd.ipp::draw_scanline_tex` (= sm2-emu `polygon.frag`):

1. 4-bit texel → `t8 = texel<<4`.
2. `curve = lumaram[lumabase + (t8>>1)]`, `lumabase = (texheader[1]&0xff)<<7`.
3. `shade = min(curve * polygon_luma / 256, 0x3f)`.
4. **colour is INDIRECT**: `colorbase = (texheader[3]>>6)&0x3ff` selects a palette
   slot, and the real 5-5-5 colour is `palram[colorbase + 0x1000] & 0x7fff`
   (model2rd.ipp:270). The low 0x1000 palette words are the **2D tilemap** palette.
5. per channel `out = colorxlat[region + chan5*0x100 + shade] & 0xff`, regions at
   word 0x0000 (R) / 0x2000 (G) / 0x4000 (B) (model2rd.ipp:252-254).
6. each channel through VF2's fixed monitor gamma `max((v-64)*255/191, 0)`
   (model2_v.cpp:2390-2394 — a constant, "bias 64, gain 51" for vf2; reproduced in
   native code, not emulated).

## Bugs found and fixed [MEASURED]

- **Decor rendered bright red** — the colour read was direct (`palram[colorbase]`)
  instead of the `+0x1000` indirection. The low palette region is the reddish 2D
  tile palette, so the whole stage was tinted red. Fixing it turned the pagodas to
  grey stone, the floor to tan, Akira's skin/navy/red and Lau's green all correct.
  Decor mean error 57.2 → 40.8, overall 52.5 → 41.8.
- **Missing monitor gamma** — adding the fixed bias-64/gain curve corrected a +17.8
  blue bias (render blue 65 vs screen 48). Fighters: Akira 45.7 → 31.3, Lau 41.4 →
  37.6.
- Two correctness fixes with no effect on *this* frame but faithful to MAME, kept:
  the 0x06 texparam stride is **two words per entry** (`[param][coef]`, not one), and
  `dotp` (front/back face) uses the polygon's own new vertex, not the strip base.

## Result vs screen.png [MEASURED]

`geo_overlay.py colourerr` (new mode; scores only pixels the 3D render drew, so the
2D sky/HUD the 3D-only render never produces is excluded; mean of |ΔR|,|ΔG|,|ΔB|):

```
akira   17018 px  mean colour error  31.3 / 255
lau     25747 px  mean colour error  37.6 / 255
decor   57449 px  mean colour error  51.7 / 255
ALL    100214 px  mean colour error  44.6 / 255
```

Colour **balance** is exact everywhere (decor towers render 22/20/14 vs screen
76/71/48 — a uniform ~0.29× brightness, same across R/G/B), which proves the
palette/colorxlat path. The residual is brightness, concentrated in the decor.

## Known limitation — persisted texparams absent from a one-frame capture

The per-index diffuse/ambient come from command **0x06**, which **persists in the
geometry engine across frames** (MAME value-inits `geo_state` to 0/0 in `geo_init`
and only ever overwrites it). This frame contains **no 0x06** (confirmed: 0 seen, and
6 identical 0x0a light writes, light = (-0.873,-0.947,-0.175), |light| = 1.30), so
the real **ambient** term — set in an earlier frame — is unavailable. We fall back to
full diffuse / zero ambient: that renders the fighters faithfully (they are
diffuse-lit) but under-lights decor faces angled away from the light, which in-game
are lifted by the persisted ambient. Forcing full brightness confirms the mechanism
(decor 51.7 → 42.3 but Akira 31.3 → 43.2). Specular, mip, bilinear and
perspective-correct UV are also still deferred. To close the decor gap, capture the
geometry engine's `texture_parameters` (or a frame carrying its 0x06), no re-decode
needed.

## Next

Capture/supply the persisted texparams to lift decor ambient; then specular
(geo_parse_np_s) and perspective-correct UV. `0x581d50` / `0x1a9b54` stay excluded.

gcc and clang build clean and produce a **byte-identical** colour PNG; the geometry
differential + display tests still pass; no exporter (`ext_emit_piece`) changed. No
capture/texture/PNG data committed.
