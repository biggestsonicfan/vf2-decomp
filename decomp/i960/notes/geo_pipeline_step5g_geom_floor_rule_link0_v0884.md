# geo_pipeline_step5g_geom_floor_rule_link0_v0884 — geometry-based floor-up rule; link-0 cull verified on the tile path

**Geometriser pipeline, Step 5 (decor).** kazuyette, reviewing textured `stage_04_full`
(dallage/murs/enseignes/statues now OK):

1. "Zigzags au sol derrière le mur à créneaux du fond" + "longues échardes le long du mur
   de droite" — looks like link-0 spikes `((attr>>8)&3 == 0)`; hypothesis: the repeat-tile
   path (Step 5d/5f) emits polys before/without the Step-4k link-0 filter. Count link-0
   polys emitted per stage (must be 0).
2. Central slab `a9bea0` (`tile3`) is uniform dark grey, no visible pattern — check its UVs
   cover the tile, not one texel; compare with `stage_04b/screen.png` centre.
3. Then the geometry-based floor rule for stage 3, as planned.

## 1. Link-0 cull — hypothesis disproven [MEASURED]

Added a per-stage counter (`ext_link0_seen`/`ext_link0_emitted`, printed by `geo-stage`:
`link-0 polys: seen=N emitted=0 (must be 0)`). The link-0 cull is a single guard
`if (((attr>>8)&3)!=0) emit;` around the **only** face-emit path (`ext_emit_poly`); the
repeat-tile path changes a poly's material/UV, not whether it emits, so it is culled on
exactly the same guard. Measured, single-frame **and** from-ROM:

      stage 04b  seen=66  emitted=0      stage 05  seen=57  emitted=0
      stage 04b  seen=122 emitted=0 (fr) stage 03  seen=86  emitted=0

**emitted=0 everywhere.** So the zigzags/échardes are not link-0 spikes. Scanning the OBJ
for thin triangles: they are short (2–8 units, thickness 0.06–0.29), on **captured** as
well as recovered objects, clustered at the walls (|x|≈16) and the back (z≈−16..−17) —
i.e. the thin crenellation posts (`rec5a03a8` = the battlements, XZ=2.0 wide) and wall
trim, which are genuinely narrow ROM geometry, plus likely z-fighting where recovered
back-side floor/wall polys sit coplanar with captured ones. Pinning down the exact visual
needs a render pass; it is **not** a link-0 or tile-path bug.

## 2. a9bea0 is textured, just dark [MEASURED]

`stage_tile3.png` is a real 256×256 texture: mean (76,76,84), stdev ~28, min 0 max 128 —
dark cobbled stone, not a single texel/flat colour. `a9bea0`'s UVs span u 0→12, v −11→1
(~12 tile repeats each axis), so the quad samples the whole tile many times over. The
MAME `screen.png` arena centre is itself dark (mean (46,49,38), stdev ~26). The slab reads
as near-uniform dark grey because the stone is **dark and low-contrast**, faithfully — not
because the UVs collapse to one texel. No change.

## 3. Geometry-based floor-up rule [FIXED, replaces Step 5f's normal test]

Step 5f flipped a tiled floor to +Y only when its **stored** model normal pointed down.
That missed stage-3 floor `rec57fd08`, whose stored normal is near-horizontal/degenerate
(VF2 normals are not consistently outward). Replaced with a **geometric** test: compute
the polygon's own normal (draw order p1,p0,vA, object space → world via `mtx`), and if it
is near-horizontal (`|ny| > 0.9·|n|`) the poly is a floor, so force its orientation to +Y.
Walls (near-vertical geometric normal) and fighters (never tiled) are untouched.

Verified across all 11 stages (correct cross-product Y-component): every real flat tiled
floor now winds up, `rec57fd08` included. The only remaining down-winding flat tiled faces
are **degenerate zero-area slivers** (one XZ dimension = 0, area < 1 — edge trim seen
edge-on, winding is float noise, zero projected area, invisible). a9bea0 still up (its
geometric normal is ±Y, so the geometric rule subsumes the old normal rule).

## Build / export

gcc + clang clean (`-Werror -Wconversion -Wshadow`); geo/display/geometry tests 5/5;
fighter path byte-identical (akira — the rule is gated on `poly_tiled`, stage-only). All
11 `stage_NN` + `stage_NN_full` re-exported; every MTL passes the texture-existence check.
No ROM/capture/OBJ committed (stages local in `..\snap\stages_obj_colour\`).

## Open (unchanged from Step 5f)

Recovered-decor truly-unbaked atlas texels (stage-4 `rec59afe0`/`rec59b2e0`): an
`ext_emit_piece` (UV) vs `ext_colour_sheet_piece` (bake) `th_ptr`/UV divergence, still to
be root-caused. The zigzag/écharde visual (if not just z-fighting) may want a render-based
confirmation.
