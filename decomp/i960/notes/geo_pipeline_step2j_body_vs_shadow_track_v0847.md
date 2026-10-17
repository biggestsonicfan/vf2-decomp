# geo_pipeline_step2j_body_vs_shadow_track_v0847 — body drifts, shadow tracks

**Geometriser pipeline, Step 2j.** After the camera-lag hypothesis was refuted
(2i), kazuyette asked, across the 30 frames and per fighter, to trace three screen-x
positions — projected body centre, projected shadow centre, and the real on-screen
position (the screenshot shadow = the green crosses) — to see which of the body or
shadow projection tracks the real fighter (the one that drifts away is wrong); to
compare Akira (advancing) vs Lau (near-still); and to check whether the body
matrices actually change frame-to-frame or stay frozen.

## Tool

`geo_camtest.py track <ram9.bin> <readstarts.txt> <shotdir>` (stdlib; imports
`read_png`/`shot_shadow_centroids` from geo_overlay.py). Per frame it splits body
and shadow objects by Tx sign (left/Akira, right/Lau), projects each object's
matrix translation to screen x, reads the matching screenshot and detects the
left/right floor-shadow centroid as ground truth, and prints bodyX, shdwX, realX
plus the frame-to-frame deltas. Local only.

## Result — the SHADOW tracks the fighter, the BODY drifts right [MEASURED]

Lau is the clean case (he barely moves, so his ground truth is stable):

| | frame 0 | frame 20 | frame 29 |
|---|---|---|---|
| real (screenshot) | 372 | 360 | 368 |
| projected shadow | 371 | 390 | 386 |
| projected body | 377 | 402 | **481** |

Lau's **real feet stay ~365 and his projected shadow stays ~380** (close), but his
**projected body marches 377 -> 481 (+104px)** over the sequence — and he is barely
moving. Akira (advancing) shows the same shape, larger: over all frames the
shadow-to-real error is smaller than the body-to-real error on **both** sides:

- side L (Akira): body-real MAE **126px**, shadow-real MAE **86px** -> shadow wins
- side R (Lau):   body-real MAE **42px**,  shadow-real MAE **18px** -> shadow wins

So the **projected shadow stays with the real fighter; the projected body drifts
rightward away from it**. The body is the wrong one. (The L-side absolute realX
~135 is a bit left of Akira because `shot_shadow_centroids` also catches dark stage
floor, and the Tx-sign split mixes objects once a fighter's parts cross x=0 late in
the sequence; but the trend — shadow near real, body drifting right — is robust on
both sides, and Lau's clean numbers settle it.)

## The bodies are not frozen — they over-advance ~2-3px/frame [MEASURED]

Body matrices **do** change every frame (mean |dBodyX| ~3.4px L / 4.3px R), while
the real fighter and the shadows move ~1px/frame. So the body translations advance
~2-3px/frame **more than they should**, accumulating to the ~90px body-shadow gap
by frame 29. It is a time-integrating rightward drift of the body matrices, not a
frozen matrix and (2i) not the camera.

## Lau drifts too — it is systematic to the body matrices [MEASURED]

Lau is near-still (real ~365 throughout) yet his projected body still drifts right
+104px. So the defect is **not** specific to the advancing fighter; both fighters'
body matrices carry the same accumulating rightward drift. That rules out "it is
just Akira's forward motion" and points at a shared body-matrix build step.

## Next

The wrong quantity is the fighters' **body** matrices (the shadows are ~right).
Walk the i960/TGP path that builds the body matrices and look for a per-frame
accumulation / integration in their translation (world X): something that adds a
small X each frame to the body transform but not to the shadow or decor transform.
`0x581d50` / `0x1a9b54` stay excluded. Then camera-free fighter/scenery split +
export.

No C changed this step (pure geo_camtest.py addition); gcc and clang build clean,
vf2_geo_display + the geometry differential tests pass; geo_camtest.py parses. No
capture/screenshot data committed.
