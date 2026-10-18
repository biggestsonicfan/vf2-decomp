# geo_pipeline_step4o_layered_decal_lift_v0873 — lift stacked translucent decals by layer (Wolf's black eye)

**Geometriser pipeline, Step 4 (colours), OBJ defect.** kazuyette, in Blender: on
Wolf, the right eye (screen-left, the mirror side) is a black zone in the middle of
the red war-paint; the left eye is correct. The face geometry is complete in Solid
view -- the black comes from the decal, like Lau's front motif (Step 4l), but this
time it is not the luma.

## Diagnosis [REFERENCE-CHECKED + MEASURED]

Dumping Wolf's head record `0x22742c`: the face carries two stacked translucent
decals -- the war-paint (texheader rect (0,384) 128x128, prims 133-187) and, drawn
after it, the eye irises and mouth (rects (0,768)/(64,768)/(128,768) 64x64, prims
188-217). Both decals are translucent (renderer bit0) and both were lifted the same
`-0.003` off the opaque skin, so they end up **coplanar**. Checked both eyes:

- The atlas bakes both iris tiles correctly (brown/green iris + white) and the
  war-paint fully red and left/right symmetric; both eyes' UVs, alpha cut-outs,
  per-poly luma (all 255 on the irises) and even the lift normals are symmetric.
  Nothing about either eye's *data* is wrong.
- The fault is the coplanar z-fight between the two decal layers. In-game MAME
  paints translucent polygons in list order, so the iris (later) draws over the
  war-paint; as coplanar static faces the two fight, and a z-buffered viewer
  resolves a mirrored pair differently per side -- one eye keeps the iris, the
  mirror eye drops to the war-paint's cut-out, which over no opaque backing shows
  the world (black). A deterministic viewer hid the iris on *both* eyes (the
  earlier war-paint won the tie); Blender's resolve made it asymmetric -> a black
  eye.

## Fix

`ext_emit_piece` now ranks a piece's translucent decals by first-seen texheader
rectangle (draw order) and lifts each successive layer a further `0.0015` out:
`deps = -0.003 - 0.0015*layer`. Rank 0 is the base translucent layer (war-paint),
so the iris (rank 1) sits in front and wins the z-test everywhere, reproducing
MAME's painter order as a static offset. `0.0015` ~ 0.1% of a fighter -- far below a
visible gap, well clear of z-fighting. Opaque polygons are unchanged; a piece with a
single translucent rect (one layer) is byte-unchanged, so only stacked decals move.
The change is in the OBJ exporter only; the scene render (`command_geo_colour`,
MAME-exact) is untouched, so scene colour error is byte-identical.

## Validation [MEASURED]

- Wolf: both war-paint eyes now show the iris (brown/green + white) cleanly and
  symmetrically, under alpha-clip over a black world (the Blender case); the black
  right eye is gone. 20 pieces / 2329 triangles (unchanged).
- No regression: Lau's back dragon and front green motif (Step 4l) stay complete and
  symmetric; Akira's gi/belt/headband unchanged; all ten fighters keep their Step 4n
  piece/triangle counts exactly (lift does not change topology). Faces reviewed
  front-on for the mirror-eye case -- akira, jacky, sarah, pai, wolf all show
  symmetric eyes; lau/shun/lion/jeffry are turned in their capture pose (side/back),
  nothing to fault.
- Control: jeffry_akira's Akira P2 still decodes to the same geometry as colour_f's
  Akira (4883 verts, 1295 faces).

gcc and clang build clean under `-DVF2_WARNINGS_AS_ERRORS=ON`; targeted
geo/display/geometry tests pass. No capture/texture/PNG/OBJ committed; the ten OBJs
in snap/fighters_obj_colour are local only.

## Next

The per-layer lift assumes the first translucent rect in a piece is the base layer
and distinct rects are distinct layers -- true for the face decals seen so far
(war-paint then eyes). A piece that stacks two decals sharing one rectangle would
still z-fight; none has turned up. `0x581d50` / `0x1a9b54` stay excluded.
