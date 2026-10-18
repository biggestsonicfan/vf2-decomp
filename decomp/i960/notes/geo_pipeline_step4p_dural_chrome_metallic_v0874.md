# geo_pipeline_step4p_dural_chrome_metallic_v0874 — Dural's chrome: mode-1 specular on a dark base, moved to a metallic MTL

**Geometriser pipeline, Step 4 (colours), new fighter + material.** kazuyette added
`snap\colour_akira_dural\` (Akira P1 left vs Dural P2 right, forced via the P2 select
cursor; frame 8837, readstart 0). Export `dural.obj` and explain her chrome look.

## Finding: no new render path [REFERENCE-CHECKED + MEASURED]

Dural is dark chrome -- a near-black body with bright silver specular highlights. The
MAME-exact scene render (`geo-colour`) reproduces it (622 specular polygon-terms; the
highlights land where the screenshot's do). So the chrome is just the **geo_mode-1
specular path we already implement** (`gc_poly_luma`, MAME `geo_parse_np_s`:
`spec = (2·dotl)·Nz − Lz`, squared per `spec_ctrl`, scaled by `spec_scale`), on a dark
base colour (her `colorbase` points at a near-black palette entry). No environment
map, no new palette, no new texture mode.

Her materials, from the persisted 0x06 table (`tparams`, shared by the whole roster):
she uses only **tpi 1** (diffuse 127 / ambient 63 / **spec_ctrl 0** -- the matte face,
feet, waist, 84 polys) and **tpi 8** (diffuse 87 / ambient 28 / spec_scale 255 /
**spec_ctrl 7** -- the chrome body, 1300 polys). The only indices in the whole table
with `spec_ctrl != 0` are 8, 25, 28 (all ctrl 7, scale 255); **no other fighter uses
any of them** (every index the other ten reference has `spec_ctrl 0`, i.e. zero
specular). So "strongly specular" is exactly `tp_sctrl[tpi] != 0`, and it is Dural-only.

### Dural measures [MEASURED]
- `geo-fighter dural-r`: 17 pieces / 2254 triangles, bbox 0.87×1.50×0.62 (complete).
- VF2_CASE_CHECK: 50 candidates, **det<0 = 0**, 2 unassigned (harmless outliers).
- geometry overlay vs screen.png: 237.4 at dx=0 dy=−3 (the usual ~1-frame lag).
- scene colour error vs screen.png: Akira 32.0 · **Dural 32.0** · decor 40.8 · ALL 37.5
  /255 -- in line with the rest of the roster.

## Fix: don't bake view-dependent specular; mark the material metallic

A specular highlight is view-dependent, so baking it on one frame and viewing the OBJ
from another angle is wrong. So:
- **Atlas bake** (`gc_poly_luma` gains `no_spec`, passed by `ext_colour_sheet_piece`):
  the atlas now carries only the matte diffuse+ambient base. For `spec_ctrl == 0`
  materials this changes nothing (their specular was already zero), so the other ten
  fighters' atlases are byte-identical.
- **MTL** (`ext_emit_piece` + `command_geo_fighter`): a poly whose material has
  `spec_ctrl != 0` is emitted under its own `usemtl sheetN_specM` (M = tpi); the MTL,
  now written after the mesh from the set of chrome materials actually used, declares
  that material metallic. Shininess is derived from the 0x06 table -- `spec_ctrl` →
  exponent (1/2/4/8 squarings) → `Pr`/`Ns`, `spec_scale` → `Pm`/`Ks`. Blender's OBJ
  importer reads `Pm`/`Pr` (Principled metallic/roughness); `Ns`/`Ks` are the legacy
  fallback. Dural's chrome (tpi 8) → `Pm 1.000 Pr 0.113 Ns 800`; her matte face/feet
  (tpi 1) stay on the plain `sheet0` material with their own value.

Threshold is `spec_ctrl != 0`, which only Dural crosses, so the **other ten fighters'
`.obj`, `.mtl` and both `.png` are byte-identical** (verified: akira, lau, wolf match
the Step 4o output exactly).

## Validation [MEASURED]
- `dural.obj` matte atlas is dark (median luma 22/255, max 108 -- no baked highlight);
  body = `sheet0_spec8` (1300 faces, metallic), face/feet = `sheet0` (84 faces, matte).
- gcc and clang build clean under `-DVF2_WARNINGS_AS_ERRORS=ON`; targeted
  geo/display/geometry tests pass. Nothing captured/exported committed; `dural.obj`
  lives in `..\snap\fighters_obj_colour\`, local only.
- Blender Material Preview check (the chrome now shading live off the viewer) is
  kazuyette's to confirm.

## Next
Derived `Pm`/`Pr`/`Ns` are a first pass (chrome = full metal, sharp); kazuyette's
Material-Preview look may want them tuned. The baked matte still has the frame's
diffuse direction cooked in (per-poly flat luma, same as every fighter) -- only the
specular was removed. `0x581d50` / `0x1a9b54` stay excluded.
