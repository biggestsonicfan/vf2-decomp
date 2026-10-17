# geo_pipeline_step3f_decoder_truncation_fix_v0857 — decoder truncation (0x00040401)

**Geometriser pipeline, Step 3 (decoder).** kazuyette's lead: Akira's trousers reach
the ankles, so the "thighs" `0x13b998` / `0x13c02c` / `0x62f7d8` are the whole
trouser leg decoded **truncated** -- they stop dead at y 0.36 and looked like jagged
lamellae. Asked to compare the polygon decode against MAME geo_parse_np_s /
geo_parse_nn_s (end bit, link / vertex reuse, triangle flag, normal skip), cross-
check sm2-emu, and fix only a proven discrepancy.

## Polygon-by-polygon comparison [MEASURED]

New gated trace (`VF2_PIECE_DBG=<rec>`): `ext_emit_piece` on `0x13b998` decoded
**10 polygons** then stopped at prim 10 on **`attr = 0x00040401`**. My loop had an
**extra** end condition `attr == 0x00040401u`. But `0x00040401 & 3 == 1` -- a valid
**quad**. Both references end a piece ONLY on the link-type bits being zero:

- MAME `geo_parse_np_s` (model2_v.cpp:1298-1300): `else /* attr&3==0 */ break;`
- sm2-emu `Geometrizer` (sm2-emu-ref/.../geometrizer.cpp:888): `if ((attr & 3) == 0)`

Neither special-cases 0x00040401 -- they decode it as a quad and keep going. So my
decoder truncated every piece at its first 0x00040401 polygon. (The rest of the
format already matched both references: initial edge p0/p1, 10-word blocks
attr+normal+2 points, normal skipped, triangle vs quad by attr&1, strip link by
(attr>>8)&3.)

## Fix

Removed the `attr == 0x00040401u` break; `ext_emit_piece` now ends only on
`(attr & 3) == 0`, as MAME and sm2-emu do. After: `0x13b998` decodes **41 polygons**
(ending on attr&3==0), 4x the geometry.

## Result [MEASURED + OVERLAY]

- **Akira**: 486 -> **2637 triangles** (1452 -> 7911 verts), height 0.00..1.46.
  The vertex-Y gap at 0.14..0.36 is gone (bins 541,392,221,373,419,...): the
  trousers now run continuously from waist to feet. The per-piece overlay
  (akira_pieces_f20.png) shows every leg segment covered; the full overlay
  (overlay_def_f20.png) shows Akira complete.
- **Lau**: 516 -> **2683 triangles** -- also improved (its pieces were truncated
  too); the overlay shows Lau complete.
- **Decor**: intact -- the projection PNG is byte-identical gcc/clang, and the
  overlay still registers the pagodas/pillars. Removing a wrong truncation can only
  add correct polygons (both references keep decoding past 0x00040401), so decor can
  only stay the same or improve.
- f29 is consistent: Akira 31 / Lau 23, 2637 / 2683 triangles.

The fix is in `ext_emit_piece`, so every exporter (export-pieces, export-textured,
geo-scene, geo-project, geo-fighter) now decodes full pieces. The synthetic geometry
differential tests still pass.

## Next

Eyeball the re-exported akira.obj / lau.obj in the viewer (now full-bodied). Then the
stage/decor export, and posing from other frames. `0x581d50` / `0x1a9b54` stay
excluded.

gcc and clang build clean and produce a byte-identical projection PNG + OBJs;
vf2_geo_display + the geometry differential tests pass; the viewer loads both
(akira 7911 verts, lau 8049). No capture/texture/OBJ/overlay data committed.
