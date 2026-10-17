# geo_pipeline_step2i_cameralag_test_v0846 — camera-lag hypothesis (refuted)

**Geometriser pipeline, Step 2i.** kazuyette proposed a cheap test before reading
i960 code: maybe the fighter BODY matrices (TGP-computed) use the PREVIOUS frame's
camera while the decor and the SHADOW matrices use the CURRENT camera; then the
body-vs-shadow gap should track the camera's per-frame motion, and re-composing the
body matrices with cam(n) in place of cam(n-1) should drop the bodies onto their
shadows. He also asked to exclude `0x581d50` / `0x1a9b54` from the fighter set.

## Tool

`tools/python/geo_camtest.py <ram9.bin> <readstarts.txt>` (stdlib). Walks every
frame, tracks each object's absolute 0x0b matrix (MAME-exact), classifies decor /
fighter / shadow exactly as geo-project does, excludes the two decor-matrix
records, and per frame reports: the decor (shared) matrix change n-1->n (the
camera), the per-fighter body-shadow gap in projected screen x, the gap the
camera-lag model predicts, and the gap after swapping cam(n-1)->cam(n) in the body
matrices. The camera swap `S = Ddec(n) o Ddec(n-1)^-1` cancels any constant stage
model, so it isolates pure camera motion (rotation + translation), and `S` applied
to a body translation gives its cam(n) re-composition. Outputs are local only.

## Result — REFUTED [MEASURED, 30 frames]

- **The camera barely moves.** The decor matrix Tx changes by <=0.038/frame and a
  fixed world point's image moves `camPanPx` <= 11px over frames 1-10 and **exactly
  0px** for frames 11, 16, 18-29.
- **The body-shadow gap grows regardless**, from ~0px (frame 1: L -3, R +12) to
  **~90px (frame 29: L +94, R +95)** — including across frames 19-29 where the
  camera is completely static.
- **Camera-lag predicts essentially no gap** (median -0px, **MAE 1px**), because
  the camera doesn't move. **Re-composing cam(n-1)->cam(n) does not fix the bodies**
  (gap MAE 34px before, 34px after). Correlation of the raw gap with the predicted
  cam-lag gap is only **r = +0.42**.

So the body-shadow gap does **not** follow the camera; the camera-lag hypothesis is
rejected.

## But the gap is not constant either — it grows with the action [MEASURED]

The gap rises monotonically over the capture (L: -3 -> +52 at f20 -> +94 at f29),
and the capture is exactly an action sequence: P1 holding Right, punch pulse ~f10,
kick pulse ~f20. The divergence tracks the **fighter's own movement/animation**,
not the camera. Two readings to separate next:
1. a per-pose lag — the body (or the shadow) is built from a different-age
   animation pose than the other, so they drift apart as the fighter moves; or
2. a real pose effect — if the ground shadow is a flatten of only the base/torso
   while the body centroid shifts as limbs extend during punch/kick, part of the
   gap is physical. (Check whether the shadow is a full-body flatten.)

Either way it is not a camera problem, so (per kazuyette's branch) the i960/TGP
path is next -- but hunting a body-vs-shadow POSE lag, not a camera lag.

## Next

Walk up the i960/TGP code that builds the fighter body vs shadow matrices and look
for a one-frame (or longer) POSE lag between them; confirm whether the shadow is a
full-body ground flatten. `0x581d50` / `0x1a9b54` are excluded from the fighter set
(decor matrix). Then camera-free fighter/scenery classification + export.

No C changed this step (geo-project's tx,ty,tz columns from Step 2h are reused);
gcc and clang still build clean and vf2_geo_display + the geometry differential
tests pass; geo_camtest.py parses. No capture/screenshot data committed.
