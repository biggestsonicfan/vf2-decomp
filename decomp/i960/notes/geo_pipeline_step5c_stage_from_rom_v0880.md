# geo_pipeline_step5c_stage_from_rom_v0880 — geo-stage --from-rom: recover the frustum-culled decor from ROM

**Geometriser pipeline, Step 5 (decor).** A single captured frame carries ~75% of a
stage's decor (the rest is frustum-culled). The decor is a contiguous model-ID range in
the ROM model table (`0x020e0004`, note stage_model_table_020e0004), and the capture is
a culled subset. `geo-stage --from-rom` fills the range back in, anchored on a capture.

## Tool

`geo-stage --from-rom <ram9> <readstarts> <rom-dir> <tex0> <tex1> <pose> <out-dir>
[colour-capture-dir] [texparams.txt]`. Added as a flag on `command_geo_fighter`; the
fighter path and plain `geo-stage` are unchanged (verified: akira byte-identical, stone
towers still 21 objects).

In `--from-rom` stage mode, after the normal capture extraction it:
1. reverse-maps each captured record to its model ID (`(oba & 0x7fffff)*4` over the
   model table);
2. groups the IDs into clusters (consecutive gap <= 64) -- the main decor is one dense
   cluster; far-off captured IDs (shadows/effects that share the camera matrix) form
   their own tiny clusters and are **not** expanded;
3. for each cluster's `[lo,hi]` ID range, adds every valid ID the capture lacks, with
   the camera (decor) matrix, so recovered objects land in the same camera-removed
   world frame as the captured decor. Captured objects are named `..._cap_rec...`,
   recovered `..._rom_rec...`. Colours come from the capture's atlas as usual.

## Intruder control (tools/python/geo_stage_fromrom.py)

Inclusion only catches *missing* objects; the filter catches the *extra* ones a blind
range-fill can add, from the emitted bboxes:
- **LOD duplicate**: a recovered object whose AABB overlaps a captured (or another
  recovered) object by >= 0.6 of the smaller volume -- a second level of detail.
- **Out of arena**: a recovered object whose centre falls outside the captured decor
  envelope (expanded 15%).
Flagged objects are excluded by default (`--include-flagged` keeps them, tagged
`_FLAG_`). Writes `stage.obj` + atlas and the report.

## Validation [MEASURED] -- per stage: emitted / captured / recovered / flagged

| stage | emitted | captured | recovered | flagged |
|-------|---------|----------|-----------|---------|
| 0  | 29 | 21 | 11 | 3  |
| 1  | 31 | 22 | 9  | 0  |
| 2  | 19 | 16 | 14 | 11 |
| 3  | 17 | 17 | 1  | 1  |
| 4  | 36 | 26 | 12 | 2  |
| 5  | 20 | 17 | 7  | 4  |
| 6  | 34 | 25 | 10 | 1  |
| 7  | 34 | 26 | 12 | 4  |
| 8  | 27 | 17 | 67 | 57 |
| 9  | 22 | 21 | 11 | 10 |
| 10 | 20 | 15 | 6  | 1  |

Every captured object is kept (inclusion holds by construction). Stages 0/1/4/6/7/10
recover clean decor (stage 4's full wall is a complete symmetric crenellated arena);
stages 2/8/9 have noisy clusters where the filter excludes most recovered ones (LOD
copies / out-of-arena); stage 3's capture was already complete. Saved to
`..\snap\stages_obj_colour\stage_NN_full\`, with the single-frame `stage_NN\` kept
alongside to compare. gcc+clang clean under `-DVF2_WARNINGS_AS_ERRORS=ON`; targeted
geo/display/geometry tests pass. No ROM/capture/OBJ committed.

## Note for the native port

The stage decor is submitted by the i960 **stage renderer at `0x20084..0x214d4`** (~54
unrolled `0x7c60` object submissions reading per-stage `g13` fields, with per-object
camera/LOD tests) -- there is no flat descriptor list. The native PC port will need to
reproduce that renderer (or bake its output) to drive the stage display list faithfully;
`--from-rom` is an offline stand-in (capture-anchored), not a model of that code.

## Next

A frame-sweep merge (multiple camera positions) would complement `--from-rom` where the
culled set reaches outside the captured ID cluster. The per-stage ID clusters and the
GF_IDGAP=64 grouping could be tightened once the renderer's exact enumeration is traced.
