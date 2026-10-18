# geo_pipeline_step4i_winding_orient_v0867 — orient triangle winding by the model normal (inside-out faces)

**Geometriser pipeline, Step 4, OBJ defect.** kazuyette, in Blender on both fighters: faces
appear to be missing and you see the inside of the mesh -- Lau's two fist tops (palm interior
visible), Akira's under-nose (a dark mustache-like spot; he has none) and both foot tops.
Akira's fists are fine, so it is a special case, not the whole decoder.

## Diagnosis [MEASURED] -- the four pistes, ruled out, then the real cause

1. **Decode truncation** (attr&3 / strip link / triangle P1=P0): NO. Our decode is faithful
   to MAME `geo_parse_np_s` (model2_v.cpp:1142-1144 breaks on attr&3==0, the quad/triangle
   stride is 10 words each with the triangle skipping the 2nd point, and the `(attr>>8)&3`
   link advance + quad/triangle vertex reuse match model2_v.cpp:340-367,530-554). Lau's fist
   (0x2ca0d0) and Akira's clean fist (0x0bca24) decode to the **same 146 prims with an
   identical strip-mode histogram and an identical attr sequence** (only 2 prims differ, in
   texture-header bits, not geometry) -- so the two fist meshes are structurally identical.
2. **Triangle P1=P0 degenerate**: only 2 of 266 tris are degenerate (the strip's natural
   degenerate 4th vertex), negligible.
3. **Pieces dropped during fighter-separation / shadow filtering (det=0)**: NO. Logging every
   dropped candidate shows only genuine ground shadows (flat matrix, y~-1.07) and the stage
   decor -- no fist/nose/foot piece is dropped; the fist is selected. Matrices are det=+1 (no
   mirroring).
4. **Wrongly lifted -0.003 as a decal**: NO. The fist is opaque (not translucent); a no-lift
   re-export is pixel-identical.

So **no faces are missing** (the fist mesh equals Akira's complete fist). The real cause:
**the model's polygon winding is not consistently outward** -- VF2 stores an explicit unit
normal per polygon and MAME lights/culls with it (flipping per `dotp`), so the raw vertex
order is not a reliable orientation. A bare OBJ (no `vn`) leaves a viewer to infer normals
from winding; the inconsistently-wound faces come out inside-out, and under backface culling
they vanish -- showing the mesh interior. It is view-dependent, so it lands on different
parts of Lau vs Akira (they face opposite ways), which is why Akira's fists looked fine while
his nose/feet did not.

## Fix

`ext_emit_piece` already reads the per-polygon unit normal (added for the decal lift). It now
passes it to `ext_emit_tri` for **every** polygon, which orients each triangle's winding so
its geometric normal agrees with that model normal -- swapping two corners (vertex + UV
together, so the texture mapping is unchanged) when reversed. The result is a consistently
outward-wound mesh, so a viewer computes correct outward normals and backface culling keeps
the outer shell. The decal lift still applies to translucent polygons (eps), and the scene
render / atlas / decode are untouched.

## Validation [MEASURED]

- Lau's fist (0x2ca0d0) and Akira's foot (0x135050) rendered with backface culling: now
  **solid from front/side/top** (the tops are closed), vs holes before. Akira's head/nose and
  both fighters' feet likewise close.
- Full colour render (no cull) of both fighters is unchanged in appearance (the fix only
  reorders a triangle's corners; same faces, same UVs) -- Lau's dragon/frogging and Akira's
  gi all intact.
- Scene colour error vs colour_f/screen.png byte-unchanged (OBJ-only): Akira 25.9, Lau 24.6,
  decor 38.7, ALL 32.8. akira.obj / lau.obj re-exported.

gcc and clang build clean under `-DVF2_WARNINGS_AS_ERRORS=ON`; targeted geo/display/geometry
tests pass. No capture/texture/PNG/OBJ committed.

## Next

Exporting the explicit `vn` (flat per polygon) would also give correct hard-edge shading
without relying on winding; deferred (needs a vn counter threaded past the geo-scene stub
boxes). Remaining fidelity items: mip/bilinear filtering, perspective-correct UV, texture-RAM
texheaders (0x800000). `0x581d50` / `0x1a9b54` stay excluded.
