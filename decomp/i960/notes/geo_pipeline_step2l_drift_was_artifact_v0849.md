# geo_pipeline_step2l_drift_was_artifact_v0849 — the "drift" was measurement

**Geometriser pipeline, Step 2l.** kazuyette's write-trace (vf2_fight_ram9_writes_10f)
showed the body-matrix zone is written by the copro (420 words) + 4 i960 writes
during the displayed frame, before the snapshot -- so timing is ruled out, like the
zone question in 2k. Remaining hypothesis: a **measurement artifact**, because
Akira advances toward Lau and splitting fighters by Tx sign / widest gap re-sorts
their limbs late in the sequence. He asked: (1) redo the fighter-only overlay for
f00/f15/f29 -- if at f29 the bodies fall on Akira and Lau, the drift is an artifact;
(2) re-trace with each body bound to its fighter by a STABLE property, not position;
(3) check the copro's 420 words against the 0x0b matrices.

## 1. Overlays -- the Tx-split is unstable [MEASURED]

`geo_overlay.py fighters` on f00/f15/f29: at f15 the widest-gap split collapses
(nearly all objects fall into one cluster), and at f29 the two clusters do not map
onto the two fighters. So the position-based split used in 2i/2j is an unstable
estimator mid-sequence -- it inflated the per-frame "gap".

## 2. Record-stable trace -- the drift is mostly artifact [MEASURED]

New `geo_camtest.py rtrack`: each object is bound to its fighter by its RECORD,
frozen from frame 0 by Tx sign (ties dropped), so an advancing fighter's limbs are
never re-sorted into the other fighter. Record is a stable key: the families
0x05/0x07/0x13/0x62/0x63 stay left (Akira) and 0x0f/0x15/0x16/0x56/0x69 stay right
(Lau) across the whole sequence. With this, the per-fighter counts are stable
(L~30, R~20) and the "+104px drift" largely disappears -- what is left is **real
fighter motion plus a bad ground truth**:

- The screenshots show **Lau really moves right**: ~x300 at f24 -> ~x380 at f29
  (he is pushed back as Akira lunges). The matrices encode this correctly -- all of
  Lau's families move right together (Tx ~0.45 -> ~1.3 over f24-29). So the bodies
  **track** the real motion; they are not drifting away from a still fighter.
- The apparent "+104px growing drift" (2j) came from **`shot_shadow_centroids`**:
  it reported Lau's shadow stuck at ~x368 while he actually moved to ~x380, so
  body-minus-"real" grew spuriously. The green-cross ground truth is a crude
  floor-darkness detector that does not track a moving fighter -- it, plus the
  Tx-split, produced the fake drift.

With visual ground truth the body-to-screen error is roughly **constant** at
~+90-110px across f24 and f29 (projected Lau ~+92px right of screen Lau at f24,
~+109px at f29), i.e. the **same persistent ~+85px rightward fighter offset** found
in Step 2h -- not a time-growing drift.

## 3. Copro writes == the 0x0b matrices [MEASURED]

New `geo_camtest.py coprochk` on the write trace: for every active frame the copro
words that land on a fighter matrix match the RAM9 snapshot **exactly -- 0
mismatches** (396/636 fighter-matrix words rewritten each frame, 240 unchanged and
persisting their zone value; frames 0/5/9 have no/empty interval, as F5/F9 are the
pause duplicates of F4/F8). So the 0x0b matrices we read are the copro's genuine
per-frame output -- not stale, not corrupt, consistent with the display list.

## Conclusion

kazuyette is right: the "growing drift" was a measurement artifact -- an unstable
position split plus a ground-truth shadow detector that could not follow the moving
fighters. The matrices are correctly captured and copro-consistent, and the
projected bodies track the fighters' real motion. The genuine remaining problem is
the long-standing **roughly-constant ~+85px rightward offset** of the fighters
relative to the decor (Step 2h) -- not timing (2k), not zones (2k), not drift.

## Next

Return to the constant ~+85px fighter-vs-decor offset with reliable ground truth:
replace `shot_shadow_centroids` with the fighters' actual on-screen position (or
read it off the screenshots), measure the offset per frame, and check whether a
single translation of the fighter world-X (or a decor-vs-fighter frame difference)
removes it. 0x581d50 / 0x1a9b54 stay excluded.

No C changed this step (geo_camtest.py rtrack + coprochk modes only); gcc and clang
build clean, vf2_geo_display + the geometry differential tests pass; geo_camtest.py
parses. No capture/screenshot/overlay data committed.
