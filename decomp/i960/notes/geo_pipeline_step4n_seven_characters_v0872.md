# geo_pipeline_step4n_seven_characters_v0872 — export the remaining seven fighters

**Geometriser pipeline, Step 4.** kazuyette added four VS-mode captures (both fighters idle,
separated), same layout as colour_jacky: `colour_sarah_shun` (f3653), `colour_pai_lion`
(f9164), `colour_wolf_kage` (f14567), `colour_jeffry_akira` (f20018). Export the seven
characters (sarah, shun, pai, lion, wolf, kage, jeffry); the jeffry_akira Akira P2 is a control.

## Tool change

`geo-fighter` already took a `-l`/`-r` suffix to pick the screen side (Step 4m). It now strips
that suffix for the output filenames, so `geo-fighter sarah-l` writes `sarah.obj` /
`sarah.mtl` / `sarah_tex0/1.png` (not `sarah-l.*`). One commit; no other behaviour change.

## Validation [MEASURED]

Per character: pieces / triangles, geometry-overlay score on that capture's screen.png
(all at dx=0 dy=-3, the ~1-frame lag), and VF2_CASE_CHECK. Colour error is the scene vs
screen.png over the P1 box (90-215) and P2 box (280-420).

| capture        | char (side) | pieces | tris | overlay | colour err |
|----------------|-------------|--------|------|---------|------------|
| sarah_shun     | sarah (L)   | 24     | 2305 | 166.1   | 33.2       |
| sarah_shun     | shun  (R)   | 25     | 2287 | 166.1   | 30.7       |
| pai_lion       | pai   (L)   | 28     | 2386 | 166.4   | 37.3       |
| pai_lion       | lion  (R)   | 30     | 2282 | 166.4   | 31.9       |
| wolf_kage      | wolf  (L)   | 20     | 2329 | 152.3   | 31.5       |
| wolf_kage      | kage  (R)   | 32     | 2250 | 152.3   | 27.2       |
| jeffry_akira   | jeffry(L)   | 30     | 2336 | 154.3   | 28.5       |
| jeffry_akira   | akira (R)   | 31     | 2293 | 154.3   | 30.0       |

- **CASE_CHECK across all four captures: det<0 = 0** (no reflection matrices on any
  character), **1-2 unassigned** candidates each (small outliers not attached to either
  fighter; every model renders complete), transparency present and handled by the translucent
  path (alpha cutout + decal lift + two-sided decal luma).
- **Control**: jeffry_akira's Akira P2 and colour_f's Akira P1 decode to the **same geometry**
  -- 31 objects, 1295 faces, 2293 triangles each -- confirming the pipeline is capture- and
  pose-independent (palette aside).
- Watch-points render clean (Solid/alpha): Sarah's bob and Pai's long black ponytail, Shun's
  white beard and glasses, Kage's mask with the mesh lower-face scarf -- no spikes, holes or
  inside-out faces (the Step 4f-4l fixes carry over). Pai's back phoenix and Shun's robe
  characters (translucent decals) are complete and symmetric.

gcc and clang build clean under `-DVF2_WARNINGS_AS_ERRORS=ON`; targeted geo/display/geometry
tests pass. No capture/texture/PNG/OBJ committed; all ten OBJs (akira, lau, jacky + the seven)
live in snap/fighters_obj_colour, local only.

## Next

No det<0 case has turned up yet, so the reflection-matrix path stays untested. The 1-2
unassigned candidates per capture are worth a glance if a body part ever goes missing.
`0x581d50` / `0x1a9b54` stay excluded.
