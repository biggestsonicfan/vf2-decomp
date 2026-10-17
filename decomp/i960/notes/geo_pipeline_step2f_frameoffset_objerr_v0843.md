# geo_pipeline_step2f_frameoffset_objerr_v0843 — frame offset + per-object error

**Geometriser pipeline, Step 2f.** On the corrected `overlay_f20` kazuyette found
the decor and Akira right, Lau covered on the body but with some pieces spilling
off to the right; he did not see the "1.5x too close" I had claimed. Before
analysing matrices he asked for two checks: (a) a +/-1 frame test between RAM9 and
the screenshot (project f20 against screenshots f19 and f21), and (b) a per-object
error to pinpoint which pieces are out of place. Both are measured on the new
capture `vf2_fight_ram9_screen_30f` (RAM9 + matching 496x384 screenshots, local).

## (a) No frame offset — RAM9 n matches screenshot n [MEASURED]

Projected frame 20 (corrected xoff=84/yoff=130) edge-aligned against three
screenshots (`geo_overlay.py score`, higher = better):

| screenshot | best edge-align score |
|---|---|
| f19 | 150.7 |
| **f20** | **156.4** |
| f21 | 154.0 |

f20 wins cleanly, so RAM9 frame *n* corresponds to screenshot *n* — no one-frame
lag. (This re-confirms the Step 2d temporal test, now at the corrected offsets.)

## (b) Per-object placement error [MEASURED]

New `geo_overlay.py objerr <proj.png.csv>` reads the per-object CSV `geo-project`
already writes and reports, per object: how far its projected bbox spills off the
visible [0,W]x[0,H] canvas, whether it is **degenerate** (a vertex projected near
Z=0 explodes the bbox past 2x the canvas), and its horizontal offset from the
fighter cluster it belongs to. Frame 20 (113 objects: 25 decor, 55 fighter, 30
shadow after the flatness split):

**The spilling pieces kazuyette saw are two broken objects, not a whole fighter:**

| obj | rec | mode | cx,cy | off-canvas px | bbox_x | note |
|---|---|---|---|---|---|---|
| 10 | `0x581d50` | 1 | 244,494 | 18578 | [-7931..10803] | degenerate, vertex at Z~0 |
| 27 | `0x1a9b54` | 1 | 116,287 | 350 | [97..846] | spills right |

Only **1 / 55** fighter objects is degenerate and **1** more spills off-canvas;
the other 53 project inside the frame. So the "pieces overflowing to the right"
are these specific records (chiefly `0x581d50`, a near-Z=0 blow-up), not Lau's
body as a whole. 13 / 25 decor objects are degenerate too — those are the big
floor/backdrop quads whose far vertices sit at the horizon (Z~0); they fill
correctly because the rasteriser clips, but their bbox is meaningless, so bbox
stats must ignore degenerates (the tool does).

**The two fighters project too close and right-of-centre [MEASURED].** Splitting
the 53 solid (fully on-canvas) fighter objects at the widest gap in projected x:

- cluster A (Akira, left): 30 objs, centre **cx=280**, x[227..312]
- cluster B (Lau, right): 23 objs, centre **cx=384**, x[341..452]
- on-screen separation **104px**

Both clusters sit right of the screen centre (x=248) — Akira, who is on the left
on screen, projects to cx 227..312, overlapping Lau's left edge. The independent
shadow landmarks (`geo_overlay.py fit`, f20) give the cleanest spread measure:
shadows project **138px** apart (L cx=190, R cx=328) but are **225px** apart on
screen (L=135, R=360). Across f00/f10/f20/f29 the left shadow always needs to move
~50px further left and the right one ~40px further right: a **compression toward
the centre**, scale **1.54** (translation+scale LS, RMS 23px) vs a translation-only
RMS of 46px. So the residual is a genuine ~1.5-1.6x horizontal compression of the
fighters' world positions about the projection centre, plus the two broken records
above — and it is fighter-specific (the decor, same projection, registers).

## Next

Find why the two fighters' world X is compressed ~1.5x about the centre while the
decor is exact: compare a decor vs fighter object's matrix translation (Tx,Tz) and
the raw-vertex scale vs matrix column norm (fighter column norms 0.237..1.357 vs
decor 1.000), and check whether `geo_object_data` applies a per-object or global
scale the walker misses. Separately, chase the two degenerate records `0x581d50`
/ `0x1a9b54` (Z~0 vertices — likely a vertex-decode or matrix-depth error on those
objects). Then the camera-free fighter/scenery classification + export.

gcc and clang produce a byte-identical projection PNG; the synthetic walker test
still passes; `geo_overlay.py` parses. No capture/screenshot/projection data
committed.
