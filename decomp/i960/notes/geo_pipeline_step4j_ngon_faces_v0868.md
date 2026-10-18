# geo_pipeline_step4j_ngon_faces_v0868 — emit quads as 4-gon faces, not pre-triangulated (inward spikes)

**Geometriser pipeline, Step 4, geometry defect.** kazuyette, in Blender Solid view (no
texture/alpha/culling): Lau's fist tops are open with triangles pointing inward (vertices
"badly connected"), and Akira's two shoe tops have a bevel slit you can see into. Akira's
fists are fine -- a special case, not the whole decoder.

## Diagnosis [REFERENCE-CHECKED] -- the connectivity is correct; the triangulation was not

Following kazuyette's pistes:
- **Vertex source**: 0x2ca0d0 (Lau fist) and 0x135050 (Akira foot) are both **polygon ROM**
  (rec > 0x1fffc, so not the RAM-patched 0x7fff window) -- not a RAM/frame-mismatch.
- **Link modes 1/2/3 and the initial points**: verified against MAME's command-buffer flow.
  The new command sets `command_index=0` (model2_v.cpp:959) and after each polygon it rewinds
  to 8 (lines 810/822), so the two initial points land at cb[2..4]=P0(n-1) and cb[5..7]=P1(n-1)
  -- i.e. our 1st record point = P0(n-1) = p0, 2nd = P1(n-1) = p1. The quad/triangle assembly
  (cb[11..13]=P0(n), cb[14..16]=P1(n); model2_v.cpp:340-367) and the `(attr>>8)&3` link advance
  (lines 530-554: case0/2 both new points, case1 P1(n-1)=P0(n), case3 P0(n-1)=P1(n)) match our
  decode exactly. Lau's and Akira's fists decode to the same 146 prims / identical attr
  sequence. So **the vertex connectivity is faithful**; the `reverse` bit (input>>4)&1 is read
  but never used by MAME, so it is irrelevant.

The real cause: MAME scan-converts each quad as a **4-gon** (`render_polygon<4>`,
model2_v.cpp:807), but `ext_emit_piece` pre-triangulated every quad on **one fixed diagonal**
(P0(n-1)-P1(n)). For a non-convex / non-planar quad that diagonal lies outside the polygon, so
one of the two triangles is an inward spike and a thin slit opens -- which is exactly what Lau's
fist and Akira's shoe show, while Akira's fist (same topology, flatter quads there) does not.

## Fix

`ext_emit_tri` is replaced by `ext_emit_poly`, which writes each polygon as a single OBJ face
of 3 or 4 vertices in MAME's order v0..v3 = P1(n-1), P0(n-1), P0(n), P1(n) (UVs in the same
table order), so a viewer triangulates a non-convex/non-planar quad correctly. The per-face
winding is still oriented to agree with the model's unit normal (Step 4i), and a translucent
decal is still lifted outward (Step 4h). Scene render / atlas / decode untouched.

## Validation [MEASURED]

- Lau's fist (0x2ca0d0) and Akira's foot (0x135050) rendered with the quad triangulated on the
  winding-consistent diagonal (what a viewer does) + backface culling: now **solid from
  front/side/top**, tops closed, no inward spike. (A bare triangle-only renderer that splits on
  a fixed diagonal still shows the sliver -- the OBJ now carries the 4-gon so the viewer, not
  the exporter, chooses the split, as MAME does.)
- Full colour render of both fighters unchanged in appearance (Lau's dragon/frogging, Akira's
  gi intact). lau.obj is now 351 triangle + 1166 quad faces.
- Scene colour error vs colour_f/screen.png byte-unchanged (OBJ-only): Akira 25.9, Lau 24.6,
  decor 38.7, ALL 32.8. akira.obj / lau.obj re-exported.

gcc and clang build clean under `-DVF2_WARNINGS_AS_ERRORS=ON` (also guarded the `usemtl`
write for the obj==NULL candidate-collection pass); targeted geo/display/geometry tests pass.
No capture/texture/PNG/OBJ committed.

## Next

Akira's "dark spot under the nose" is colour, not geometry (gone in Solid view): to trace,
dump the computed luma + normal of the under-nose polygon and compare to colour_f/screen.png
at that pixel. `0x581d50` / `0x1a9b54` stay excluded.
