# geo_pipeline_step3_fighter_export_v0852 — export Akira and Lau (camera-free)

**Geometriser pipeline, Step 3 (export).** With the projection fixed (Step 2n),
export each fighter alone as a textured OBJ, posed at frame 20, in a camera-free
world frame (decor matrix inverted), standing, centred on the feet. Verify the
family attachment on a far frame (f29) and on the writes_10f capture.

## Tool

`vf2i960 geo-fighter <ram9.bin> <readstarts.txt> <rom-dir> <tex0> <tex1>
<pose-frame> <akira|lau> <out-dir>` (local only). It walks the pose frame, selects
a fighter's objects by **record family** (`rec>>16`):

- **Akira** = 0x05 / 0x07 / 0x13 / 0x62 / 0x63
- **Lau** = 0x0f / 0x14 / 0x15 / 0x16 / 0x29 / 0x56 / 0x69

excluding shadows (flat matrix, `gs_flatness < 0.10`) and the two decor-matrix
records 0x581d50 / 0x1a9b54. Each object's captured matrix has the camera baked in
(Step 2); the tool re-expresses it in the decor's world frame by composing with the
**inverse of the decor (shared) matrix**: `A = A_obj . A_dec^-1`,
`T = (T_obj - T_dec) . A_dec^-1` (`gs_mat3_inv` + `gs_worldframe`). It then decodes
geometry from `oba` and textures via the `tpa` UV/texheader table (as geo-scene),
in two passes: pass 1 accumulates the world vertices for a bounding box; pass 2
writes the OBJ centred on the feet -- horizontally on the bbox centre, and the
**vertical axis (longest bbox extent)** shifted so the lowest point sits at y=0.
gcc and clang produce a byte-identical OBJ.

## Family attachment is stable [MEASURED]

Same families, near-identical counts across a far frame and a different capture
(different opponent run):

| | Akira pieces | Lau pieces |
|---|---|---|
| screen_30f f20 | 24 | 18 |
| screen_30f f29 | 23 | 18 |
| writes_10f f0 | 24 | 17 |
| writes_10f f8 | 24 | 17 |

(The small wobble is one 0x13 limb and the transient 0x29 piece appearing/leaving
with the animation.) The leftover families -- 0x00 (tiny shared primitives) and
0x08/0b/1b/1d/2c/2e/a8/a9 -- are not in either fighter's set and stay out of both.

## Result

Both export clean, feet-centred, upright (vertical axis y), human-proportioned:
- **Akira** f20: 24 pieces, 518 triangles, 1554 verts, world bbox ~1.28 x 1.48 x
  0.69 (w x h x d).
- **Lau** f20: 18 pieces, 434 triangles, 1302 verts, world bbox ~0.90 x 1.41 x
  0.69.

Files (local, never committed):
- `..\snap\fighters_obj\akira.obj` (+ `akira.mtl`, `tex0.png`, `tex1.png`)
- `..\snap\fighters_obj\lau.obj` (+ `lau.mtl`, `tex0.png`, `tex1.png`)

Viewer (SDL3, built with `-DVF2_BUILD_VIEWER=ON`; binds the two texture banks by
sheet). From `vf2-decomp`:

    build/vf2_model_viewer.exe ../snap/fighters_obj/akira.obj \
        --tex0 ../snap/vf2_fight_tex0_f1808.bin \
        --tex1 ../snap/vf2_fight_tex1_f1808.bin

(and the same with `lau.obj`). `--selftest` loads headless: akira 1554 verts, lau
1302 verts, textures bound OK.

## Next

Eyeball the two meshes in the viewer (seams at the joints are expected -- per-object
world matrices, no skinned skeleton). Then the stage/decor export and, if wanted,
posing from other frames. 0x581d50 / 0x1a9b54 stay excluded.

gcc and clang build clean and produce a byte-identical OBJ; vf2_geo_display + the
geometry differential tests pass. No capture/texture/OBJ data committed.
