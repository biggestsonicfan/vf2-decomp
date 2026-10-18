# geo_pipeline_step4k_linktype0_cull_v0869 — cull link-type-0 strip-restart polygons (spikes/slits/mustache)

**Geometriser pipeline, Step 4, geometry defect.** kazuyette, in Blender Solid view: Akira's
"mustache" is a single under-nose triangle (nostril texture) whose third vertex drops below
the mouth; Lau's fist tops have inward spikes; Akira's shoe tops have a bevel slit. Same
cause for all three.

## Diagnosis [REFERENCE-CHECKED + MEASURED]

Working kazuyette's pistes and the MAME `geo_parse_np_s` oracle:
- `attr & 1` polarity is correct (1 = quad, 0 = triangle; model2_v.cpp:800/1274), the triangle
  duplicates P1(n)=P0(n) as MAME does (model2_v.cpp:365), the initial points map to
  P0(n-1)/P1(n-1), and the `(attr>>8)&3` link advance + assembly all match -- the vertex
  connectivity is faithful.
- Dumping the degenerate spike triangles: each is a polygon with **`(attr>>8)&3 == 0`** (link
  type 0), e.g. attr `0x00040401` -- `p0 == p1` (a collapsed edge) joined to a far new point,
  a huge spike (edge up to 0.37 of a ~1.5-unit fighter).

MAME's `check_culling` (model2_v.cpp:254) **culls every link-type-0 polygon** -- but the link
update (model2_v.cpp:529) is *outside* the cull guard, so the strip still advances. A
link-type-0 polygon is a strip *restart*: its drawn face spans the stale previous edge to the
new points (garbage), and it exists only to load the next strip's two base points. We were
drawing it; MAME never does. (`0x00040401` has `attr&3==1`, so it is NOT a piece terminator --
the decoder keeps going, as fixed earlier.)

## Fix

Skip emitting a polygon when `(attr>>8)&3 == 0`, while still running the strip advance, in
both the OBJ exporter (`ext_emit_piece`) and the scene render (`command_geo_colour`, next to
the existing translucent-solid skip). One line each.

## Validation [MEASURED]

- Akira's under-nose "mustache" is gone (clean skin); Lau's fists are solid with proper finger
  ridges and closed tops; Akira's shoe is solid, no slit -- all verified under backface culling
  from front/side/top (geometry, no texture). The egregious spikes (edge ~0.37) are gone.
- Both fighters stay complete (no new holes) -- link-type-0 faces are restarts, not surface.
- Scene colour error vs colour_f/screen.png improves slightly (the spike pixels go): Akira
  25.9 -> 25.7, Lau 24.6 -> 24.4, decor 38.7, ALL 32.8. akira.obj / lau.obj re-exported.

This pairs with Step 4j (quads emitted as 4-gons): 4j lets a viewer triangulate non-convex
quads; 4k removes the restart polygons that produced the big spikes.

gcc and clang build clean under `-DVF2_WARNINGS_AS_ERRORS=ON`; targeted geo/display/geometry
tests pass. No capture/texture/PNG/OBJ committed.

## Next

MAME also backface-culls single-sided rear polygons (model2_v.cpp:246-250); we keep those so
the OBJ is complete from every angle. A handful of thin-but-valid faces remain (legitimate
low-poly transitions). `0x581d50` / `0x1a9b54` stay excluded.
