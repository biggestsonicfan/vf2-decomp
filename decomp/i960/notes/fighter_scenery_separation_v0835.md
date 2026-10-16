# fighter_scenery_separation_v0835 — moving capture, matrix criterion + blocker

**celsowm slice (2026-10-16), continues `fighter_scenery_separation_v0834.md`.**
New capture `..\snap\vf2_fight_moving_10frames.txt` (local, never commit): P1
walking right + alternating punch/kick, 9 frames sampled every 10 (f10..f90).
Goal: separate camera motion from per-object motion and export `fighter1.obj`,
`fighter2.obj`, `stage.obj` by that measured criterion.

**Result: the criterion is identified and validated, but the capture cannot be
fully traversed without reverse-engineering the double-buffered geometrizer
scene graph (the Option-2-scale work). No fighter/stage export is shipped — a
guess would repeat the address-range attribution celsowm rejected in v0834.**

## The measured criterion (validated on the reachable sub-list)

The geo display list is the RAM9 buffer at `0x900000`. Reconstructing it from
the capture (replay `push_geo_data`: prg `0x804000` writes and geo-function
writes `0x800000-0x800fff`, with the write pointer set by geo `0x801008`), then
walking opcodes `(opcode>>23)&0x1f` per the MAME reference
(`third_party/mame-model2-ref`: `model2.cpp` geo_w/push_geo_data,
`model2_v.cpp` geo_process_command), gives each object its **transform matrix**
(opcode `0x0b`, 12 floats) at its `object_data` (`0x01`).

Comparing each object's matrix across the 9 frames:
- the 30 objects reachable from the captured read-start are **all
  matrix-static** (matdelta 0) — and they are the stage range `tpa 0x1b4xxx /
  oba 0x8f_xxxx`, the same cluster v0834 confirmed is scenery.
- the camera is **not** in these per-object matrices (static despite the capture
  panning), so a per-object matrix that *varies* is a clean, **camera-free**
  fighter signal. This is exactly celsowm's criterion; it works on every object
  the walk reaches.

## Why the fighters are not reachable (the blocker) [MEASURED]

- **Double-buffered.** Odd frames set read-start `0x908000`, even frames
  `0x918000`; writes fill `0x86ec..` and `0x186ec..` respectively — two
  ping-pong halves of RAM9 `0x900000-0x91ffff`.
- **Jump-table scene graph.** The buffer holds ~405 jump opcodes
  (bit 0x80000000) and a jump table at low offsets pointing to object entries
  scattered across the buffer; `geo_parse` from the captured read-start walks
  one sub-list of **30 stage objects** and ends, not the full 116 submissions.
- **Fighter transforms are front-loaded via the TGP copro, not per-object.**
  Of 116 geo `0x800010` submissions, only ~20 carry isolated per-object copro
  (`0x884000`) read-backs, and those are the **small** objects (2–16 tris =
  shadows / highlights / effects), not the fighter body meshes. The body meshes'
  transforms are computed in the front-loaded copro phase whose per-object
  attribution needs the TGP stream decoded.
- **oba is ROM for ~114/116** and the geo command changes for only 2 objects
  over the 9 frames — VF2 poses fixed ROM meshes by matrices, so neither
  source-region nor mesh-pointer change isolates the bodies.

Reaching all 116 objects' matrices therefore requires: (1) per-frame
double-buffer selection, (2) full jump-table scene-graph traversal, (3) the TGP
copro matrix path for the front-loaded transforms. That is the geometrizer
pipeline reverse-engineering (Option 2), a multi-session effort, not a slice.

## Recommendation

- **Now:** identify the fighters visually from the per-piece export shipped in
  v0834 (`vf2i960 export-pieces`), which is the ground truth and needs none of
  the above. Once a cluster is confirmed by eye, that visual proof is the
  attribution and `fighter1/2/stage.obj` can be cut from it.
- **To make the automated matrix criterion complete:** implement the
  double-buffered jump-table geo traversal + the TGP copro matrix decode, then
  classify every object by per-frame matrix variation (fighter = varies,
  scenery = static) — camera-free, as validated above on the reachable
  sub-list. This is a dedicated pipeline task to schedule explicitly.
