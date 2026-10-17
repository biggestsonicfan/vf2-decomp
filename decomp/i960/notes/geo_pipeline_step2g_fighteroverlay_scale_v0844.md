# geo_pipeline_step2g_fighteroverlay_scale_v0844 — fighter-only overlay + scale

**Geometriser pipeline, Step 2g.** My measured "1.5x too close" from Step 2f
contradicted kazuyette's reading of overlay_f20 (the blue seemed to fall on
Akira). He asked for an unambiguous overlay — only the 55 fighter objects, no
decor/shadows, thick contours, colour A (left group) / B (right group), a cross on
each group centre and on each shadow, projection and screen — to settle it, then,
if the compression holds, a decor-vs-fighter comparison and the cause of the Z~0
records. Frame 20 of `vf2_fight_ram9_screen_30f` (local capture).

## Tooling

`geo-project` now also writes `<out>.png.tris` (every filled triangle's projected
screen coords + object/group) and six diagnostic CSV columns: `rawext` (max raw,
identity-posed vertex-bbox dimension), `c0n/c1n/c2n` (matrix column norms), and
`zmin/zmax` (posed Z range). New `geo_overlay.py fighters <shot> <tris> <csv>
<out>` draws the fighter-only overlay on the dimmed screenshot: thick contours
split into cluster A (left) / B (right) at the widest x-gap, a cross on each
cluster centre (A blue, B orange), on each projected shadow (yellow) and on each
screen shadow (green). gcc/clang byte-identical PNG+CSV+tris.

## The overlay settles it — not a symmetric compression [MEASURED]

On screen: **Akira** (blue gi, HUD left) stands left at x~140-220; **Lau**
(ponytail, HUD right) stands centre at x~250-340. In the overlay the projected
fighter geometry occupies **x~227-452** (centre-to-right):

- the left cluster A (centre cx=280) lands on **Lau**, not Akira;
- the right cluster B (centre cx=384) sits in **empty space right of Lau**;
- **Akira's position (screen-left) is almost unpopulated** by projected geometry.

So it is **not** the symmetric "1.5x too close" I claimed in 2f: the fighter
geometry is shifted right / mis-placed horizontally, and the screen-left fighter
barely projects where he stands. kazuyette's overlay reading (blue on Akira) came
from the earlier *fill* overlay coloured by the x=248 midline; in this clean
cluster view the left cluster is on Lau. Projected shadows (yellow) likewise sit
inboard of the screen shadows (green, L=135/R=360): the feet landmarks are pulled
toward centre while one body cluster overshoots right.

## Decor vs fighter — what actually differs [MEASURED]

| | column norms c0/c1/c2 | raw vertex extent | posed Z |
|---|---|---|---|
| decor (25) | **1.000 / 1.000 / 1.000** | 6..97 (med 6) | 3..53 (med 16) |
| fighter (55) | **1.000** (one at 1.25) | **0.1..0.5** (med 0.3) | **4.3..4.9** (tight) |
| shadow (30) | 0.22..1.35 | 0 (flattened) | 4.4..5.0 |

- **Matrix column norms are identical (unit) for decor and fighters** — there is
  **no per-object matrix scale** distinguishing them. The earlier claim (v0842)
  that fighters have norms 0.237..1.357 was wrong: **those were the shadows**
  (flattened matrices, det~0, so non-unit columns). Corrected here.
- **Fighter model vertices are ~15-50x smaller than decor** (rawext 0.3 vs 6-97)
  and sit at a **tight close depth Z~4.7**, whereas decor spans Z 3-53. A fighter's
  on-screen size therefore comes out *right* (0.3 * focus 600 / 4.7 ~ 38px, and
  measured screen_h is 10-60px), so **size/scale is not the bug** — placement is.
- **MAME `geo_object_data` (model2_v.cpp:1715) applies no vertex scale**: it only
  selects the polygon source by `oba` bits (`0x01000000`->poly RAM1,
  `0x00800000`->poly ROM, else poly RAM0) and parses by `mode&3`. So nothing is
  missing on the scale side; the matrix translation is the only placement input.
- Source-select tally for frame 20: **111 ROM + 2 RAM0**; all 26 decor are ROM,
  the 2 RAM0 are elsewhere. VF2 characters live in ROM, so the RAM/ROM split does
  not explain the fighters (we decode ROM, which is correct for them).

Conclusion: fighters decode to sensible small models at the right size; the
horizontal error is in their **matrix translation (world X/Z)**, not vertices,
column scale, or a missing geo_object_data scale.

## The two Z~0 records [MEASURED]

`0x581d50` (oba=0x00960754) and `0x1a9b54` (oba=0x0086a6d5) are **ROM-sourced**
(not the 2 RAM0 objects) yet decode to **decor-scale geometry** (rawext 48 and 12,
unlike real fighter parts at 0.1-0.5) and their **posed Z straddles the camera
plane** (zmin -19.4/-3.0 < 0, zmax 28.5/12.0). Vertices with Z<=0 project through
the 1/Z pole, which is the x[-7931..10803] bbox explosion. So these two are not
normal fighter body parts: they are large-unit records whose matrix places them
across the eye. They are the "pieces spilling right" from 2f, and are a distinct
defect from the main-body misplacement.

## Next

The main-body horizontal error is in the fighter **matrix translations**: compare
the fighter 0x0b matrices' (Tx,Tz) against the on-screen fighter positions and
check whether VF2 composes fighter matrices from a base + relative transform that
the walker flattens wrongly (Step 2c saw only 0x0b writes, no 0x0c/0x0d/copro, so
re-examine how the host builds those 0x0b matrices). Separately, identify what
`0x581d50` / `0x1a9b54` actually are (large-unit records across the eye) and
whether they should be culled before the fighter split. Then camera-free
fighter/scenery classification + export.

gcc and clang produce byte-identical projection PNG + CSV + tris; vf2_geo_display
and the geometry differential tests pass; geo_overlay.py parses. No capture/
screenshot/projection data committed.
