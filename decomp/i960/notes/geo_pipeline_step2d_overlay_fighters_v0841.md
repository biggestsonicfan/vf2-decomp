# geo_pipeline_step2d_overlay_fighters_v0841 — screenshot overlay, fighter split

**Geometriser pipeline, Step 2d.** New reference capture (local, never committed)
`vf2_fight_ram9_screen_30f.bin`+`.txt` (same format as p1move + a `snapindex`
column) with a matching MAME screenshot per frame in
`vf2_fight_screen_30f/fXX_frameNNNN.png` (496x384, same frame-done notifier).
Screen: **Akira (P1, left) vs Lau (P2, right)**, round 1, P1 advancing +
punch(~idx10)/kick(~idx20). So it is NOT an Akira-vs-Akira match.

## Tools

- `vf2i960 geo-project` now renders on a **496x384 canvas** (screen size, default
  `xoff=76 yoff=40`), tracks the current window(0x03)/focal(0x09)/mode(0x07) as
  MAME does, colours each object by **projected screen side** (left/Akira blue,
  right/Lau orange, decor grey, shadow dark) using a robust on-screen centroid,
  and writes a per-object CSV `<out>.png.csv`
  (`obj,rec,group,side,mode3,cx,cy,bbox,screen_h,ntri`).
- `tools/python/geo_overlay.py` (committed, stdlib-only): `overlay` blends a
  projection over its screenshot (keeps colours + silhouette edges), `score`
  reports the best edge-alignment shift, `offsets` runs the edge score across all
  frames at temporal offsets -1/0/+1.

## Measured answers

**1. Separate fighters by position, not record range.** The anchor record range
`0x130a44..0x1434a0` (Step 1's guessed "Akira" range) **mixes both fighters**.
By projected screen side in frame 20:
- LEFT / Akira: `0x1345ac`, `0x135b48`.
- RIGHT / Lau: `0x13b998`, `0x13c02c`, `0x135050`, `0x1365c4`, **`0x130a44`**.

**`rec 0x130a44` projects to cx=333 (RIGHT) = LAU**, not Akira (mode 1, group
fighter). kazuyette independently recognised Lau's ponytail in the 3D scene. So the
earlier "0x130a44 = Akira's face" label (from a different capture) does not hold
for this scene; fighters must be assigned by position + overlay.

**2. Décor is correct, fighter bodies are not.** Décor (stage, rec
`0x3c1fac..0x3ca730`) projects onto the pagodas/floor correctly — so the matrix,
focal (600,600), window-centre (248,319) and perspective are right. Shadows
(flat, det3~0) project correctly to each fighter's feet: Akira x~134..240, Lau
x~284..337. But the fighter **bodies** project too high / too small / shifted
right: the non-shadow "fighter" objects cluster at y~60..180 while the on-screen
fighters span y~130..320.

**3. The frame offset is not the cause.** `geo_overlay.py offsets`: mean
edge-align score t=-1 97.3, **t=0 97.9**, t=+1 97.5 -> RAM9 frame n matches
screenshot n, no one-frame lag.

**4. What differs between décor and fighter objects [MEASURED]:**
- **mode (0x07 & 3):** décor = **all mode 1** (normals present). Fighters are
  mixed: ~16 mode 1, several mode 2 (**no normals** -> a different vertex stride
  in MAME's geo_parse_nn_*), a couple mode 3.
- **matrix column norms |c0|,|c1|,|c2|:** décor = **exactly 1.000** (pure
  rotation, no scale). Fighters = **0.237 .. 1.357** (per-object scale baked into
  the matrix). This scale is the one property unique to the fighter bodies that
  project wrong.
- `geo_object_data` applies no extra per-object scale; it just reads oba/obc and
  the current matrix/mode. So the size difference lives in the matrix scale
  and/or the vertex format, not in a missing scale step.

**5. Akira is severely under-represented this frame:** only 5 on-screen body
objects (+22 floor-shadow pieces) vs Lau's 50 body (+8 shadows). Most fighter
bodies project into one shifted up-right cluster, so the left/right split by
screen centre puts the bulk on Lau's side; Akira's few left-of-centre pieces are
all that remain blue. This points at a systematic fighter-body projection error
(scale / vertex format), not a per-fighter data problem.

## Next

Find the fighter-body size error: decode a décor record and a fighter record's
RAW local vertices, compare magnitudes against their matrix column norms (ratio =
scale error), and test the no-normals (mode 2/3) vertex stride
(geo_parse_nn_ns/nn_s) vs the normals-present stride the decoder currently always
uses. Fix the fighter decode, re-overlay, and expect the bodies to fill the
on-screen fighters; only then assign fighter/scenery and export.

gcc and clang produce a byte-identical projection PNG + CSV; the synthetic walker
test still passes. No capture/screenshot/projection data committed.
