# geo_pipeline_step5i_decal_front_mirror_v0886 — decal-in-front depth-bias; opaque-tile map_d; texheader mirror

**Geometriser pipeline, Step 5 (decor).** kazuyette on textured `stage_04`: (a) the big red
sign z-fights the wall and shows in pieces; (b) three small "对手" bands across the wall
instead of one sign; (c) the central stone slab reads transparent (Blender grid shows
through); (d) is the sign text mirrored?

## (a) Decal-in-front depth-bias [FIXED]

Step 5h pushed every coplanar face of an earlier-drawn object back 0.06 by draw order.
That sank a translucent sign (a decal the ROM lifts ~0.003-0.03 in front of its wall)
behind the wall, and sank the central slab under floor trim drawn later. Reworked
`geo_stage_depthbias.py` to bias only a **genuine z-fight**: faces in the same coarse
plane bucket, at the **same depth level** (within 0.0015), from **two or more distinct
objects**, whose in-plane boxes **touch** (gap < 0.5). The front-most such object stays;
each other is pushed back one 0.06 layer per distinct front-er overlapper (captured in
front of recovered, later-drawn in front of earlier). Consequences, all by construction:
a lifted decal sits in a higher depth level than its wall, so it is never grouped with it
and always stays in front; the central slab and far wall-trim share a plane but not a
place, so the slab keeps its height; abutting-only tiles (no box overlap... but boxes
still touch) are separated where they truly coincide. Visible (iso) z-fight on stage_04
dropped ~186 -> ~59 px; fighters never run through it (byte-identical, akira verified).

## (b) Sign not tiled [MEASURED, guard added]

Confirmed every tiled poly on stage 4 samples opaque stone/brick (texheader translu=0,
tex cells (1280,256)/(1536,0) = the stone/brick tiles); **no sign poly is tiled**, so the
"three bands" are not a repeat-tile artifact -- they were the sign z-fighting the wall in
pieces (fixed by (a)). As a guard per kazuyette ("pas aux décalques ni aux translucides"),
`ext_emit_piece` now never routes a **translucent** poly through the tile path.

## (c) Opaque slab was transparent in Blender [FIXED]

a9bea0 is exported correctly (8 faces, material tile3, opaque 256x256 stone image). But
Blender's OBJ importer feeds a `map_d <image>`'s **colour** (not its alpha) into the
material dissolve, so `map_d` on a dark-but-opaque tile made it ~30% opaque and the
viewport showed through it. Fix: emit `map_d` for a tile **only when that tile actually
has a transparent texel** (tracked during the atlas cut); an opaque tile gets `map_Kd`
only and stays solid. The atlas sheets keep `map_d` (they carry real cutout alpha).

## (d) Texheader mirror bits were ignored [FIXED]

`vf2_texture_header_decode` reads `mirror_x`/`mirror_y` (th0 bit 8/9) but the geometriser
never applied them -- a texture the hardware samples flipped was baked/UV'd un-flipped. In
`ext_emit_piece` (stage path only, `tiles != NULL`), a mirrored texheader now flips the UV
within the tile (`u = w - u`, `v = h - v`). Mirrored-poly counts: stage 0=19, 3=20, 5=3,
7=33, 8=3, 9=4; **stage 4 = 0**, so the stage-4 sign is not texheader-mirrored (the earlier
"mirrored" look was an atlas-rotation artifact of the inspection crop). Fighters set no
mirror bits and pass `tiles == NULL`, so their OBJs stay byte-identical (akira verified).
`geo-stage` now prints the per-stage mirrored-poly count beside the link-0 count.

## Build / export

gcc + clang clean (`-Werror -Wconversion -Wshadow`); geo/display/geometry tests 5/5;
`py_compile` clean; all 11 stages re-exported (single-frame + `_full`); akira OBJ+MTL
byte-identical. No ROM/capture/OBJ committed.

## Open

- Residual coplanar z-fight (mostly deep/occluded multi-tier floors, and `_full`
  recovered-vs-captured near-coplanar seams) is reduced, not eliminated; a float-z viewer
  may still shimmer on exactly-coincident deep floors.
- The arena "pit" floor polys bake transparent (unbaked atlas region) -- the recovered/
  captured UV-vs-bake `th_ptr` divergence, still the next task.
