# geo_pipeline_step2c_assoc_projector_v0840 — matrix assoc, anchor, projector

**Geometriser pipeline, Step 2c** (kazuyette: the two Akira exports "look like
nothing" — two distant groups + scattered pieces, even transposed; it is not a
seam question). Three things, before touching the camera.

## 1. Matrix ↔ object association — verified MAME-correct, no off-by-one

Dumped the full command sequence of `vf2_fight_ram9_p1move_30f` frame 20 (from
`m_geo_read_start_address`): the list is 258 opcodes —
`0x08 zsort, 0x07 mode×14, 0x05 poly×2, 0x09 focal×1, 0x16 lod×1, 0x0a light×6,
0x03 window×7, 0x0b matrix×112, 0x01 object×113, 0x0f end`.

**Every object command is immediately preceded by its own 0x0b matrix write**
(112 matrices : 113 objects, the extra object reuses the standing matrix). MAME's
`geo_object_data` (model2_v.cpp:1715) uses the *current* `geo->matrix`, set by the
last 0x0b/0x0c — exactly what the walker attaches. There are **no 0x0c
translates, no 0x0d data-mem-push, and no copro-written matrices** in this list,
so the matrix the walker binds to each object is the one MAME uses. **No
off-by-one.**

## 2. Geometry pointer — was decoded from the wrong field (now fixed)

MAME `geo_object_data` reads geometry straight from **oba**
(`polygon_rom[oba & mask]`, byte `(oba&0x7fffff)*4`); `tpa`/`tha` are only the
texture-point / texture-header addresses. `geo-scene` had been decoding geometry
through a `tpa`→record table lookup (a FIFO-era indirection). **Fixed: geometry
now comes from oba directly, MAME-exact; the tpa lookup is kept only for UVs.**
Measured: the lookup disagreed with oba on **2/113 slots** — so this was a real
correctness bug but NOT the main cause of the scatter (only 2 pieces moved).

## 3. The anchor cluster spans TWO world positions

`0x130a44..0x1434a0` (the range Step 1 called "Akira", anchored on the confirmed
face record 0x130a44) resolves to **18 objects at two distinct translations** in
frame 20:
- 14 slots at Tx ≈ −1.0, Ty ≈ −1.2, Tz ≈ 6.9 (one standing figure);
- 4 slots at Tx ≈ +0.8, Ty ≈ −0.3, Tz ≈ 5.4 — including slot 107, rec
  **0x130a44** (the face itself).

So the oba-range anchor **mixes two spatial groups**. Either the match is
Akira-vs-Akira (both fighters use these records, so a record-range can never
separate them — only the spatial split can) or the range is too broad. The
projector + screenshot will disambiguate. This is why the Akira OBJ "looked like
two distant groups": it is two world positions merged by record range.

## Why the world-space OBJ looks scattered

Matrix assoc is right and geometry is (now) right, yet the raw OBJ looks wrong
because it is **pre-projection** camera/world space: vertices are large (±~96)
and the real image only forms after MAME's focus + perspective divide + window
centring. Hence the projector.

## Projector — `vf2i960 geo-project` (MAME-exact)

`geo-project <ram9.bin> <readstarts.txt> <rom-dir> <pose-frame> <out.png>
[W H xoff yoff]` walks one frame tracking the current matrix (0x0b/0x0c), focal
distance (0x09) and the first window's vanishing point (0x03, center0), decodes
each object from oba, and projects every triangle with MAME `model2_3d_project`
(model2_v.cpp:657):

    sx = xoff + center.x + (x * focus.x) / z
    sy = (384 - center.y) + yoff - (y * focus.y) / z

z-buffered fill into an RGB PNG; Akira-anchor objects red, shadows dark, rest
grey. Measured on frame 20: **focus = (600, 600)** (the known VF2 camera scalar),
window-center0 = (248, 319), 113 objects, 1434 triangles. The red (anchor)
pixels land in a **compact** region x[197..448] y[170..311] (not scattered),
confirming the pose math is sane; the red's width matches the two-position
anchor. `xoff`/`yoff` default to MAME's CRTC offsets (84, 130) and are tunable
to register against a screenshot.

**Validation criterion (awaiting kazuyette's capture):** a new capture giving, per
frame, RAM9 + the matching MAME screenshot; success = the projection overlays the
on-screen characters/stage. gcc and clang produce a byte-identical PNG; the
synthetic walker test still passes.

## Next

Overlay geo-project against the provided screenshots; tune xoff/yoff/center_sel
to register; confirm whether the match is Akira-vs-Akira (anchor two positions).
Then Step 3 (camera removal + fighter/scenery classification) on a registered,
projection-validated scene.
