# geo_pipeline_step3e_polygon_ram_replay_v0856 — polygon-RAM replay; RAM != shin

**Geometriser pipeline, Step 3 (polygon RAM).** kazuyette: no new capture needed --
the polygon RAM is filled by the display list's own 0x05 commands (geo_polygon_data,
model2_v.cpp:1886). Simulate them, decode the RAM-sourced objects (0x000010 Akira,
0x000410 Lau) from the reconstructed RAM like geo_object_data, and verify Akira's
shin is now covered.

## Implemented — 0x05 replay + RAM decode [MAME-exact]

`geo-fighter` now reconstructs the polygon RAM: `gf_fill_polyram` walks the frame
and applies every 0x05 (address, count, then count words into polygon_ram1 if
address&0x01000000 else polygon_ram0, at word index address&0x7fff; each buffer
0x8000 words, LE). Each object is then decoded from the correct source by its oba
bits: bit24 -> polygon_ram1, bit23 -> polygon ROM, else polygon_ram0, at
(oba&0x7fff)*4 for RAM. f20: two 0x05 (RAM0 idx 0..200 and 256..456), both before
the two RAM objects (idx 4 and 260), both in written regions -- so building the RAM
then decoding is exact. The 2 RAM pieces now decode (previously skipped); f20 and f29
both give Akira 31 / Lau 23.

## But the RAM objects are NOT the shin [MEASURED]

Decoded, the RAM objects are **small pieces at the waist/thigh, not the shin**:
`0x000010` (Akira) -> world y **0.77..0.84**, only **6 verts** (2 tris); `0x000410`
(Lau) similar. Akira's shin gap is at world y **0.14..0.36** (screen y 277..315),
far below. So decoding the polygon RAM did **not** cover the shin -- the RAM objects
are waist/belt detail, and the hypothesis that they were the shins does not hold.

The 6-vert size is **not** a mis-decode: MAME's no-normals parser `geo_parse_nn_ns`
(model2_v.cpp:1369) also does `input += 3` (skips a normal word), so np (mode 0/1)
and nn (mode 2/3) share the same vertex stride that `ext_emit_piece` uses. These RAM
pieces are genuinely small.

## Where the shin stands

Akira's leg, after the RAM decode, is still: feet (`0x135050`/`0x1345ac`, 138 verts,
flat at world y 0.00..0.10), thighs (`0x13b998` y0.36..0.75, `0x13c02c`, `0x62f7d8`),
and **nothing at the shin y 0.14..0.36**. No ROM object, no RAM object, and no RAM1
object (there are none) covers it. Lau's shin is a ROM piece (`0xfc5f0`,
y screen 271..312) and is present; Lau's leg is complete. So Akira specifically lacks
a decodable shin.

## Next

The shin is not polygon-RAM. Compare Akira's leg records to Lau's complete leg
(`0xfc5f0` + `0x130a44`): does Akira's list lack a shin record entirely (a per-pose
or per-character difference), or is a shin record present but filtered? Also
re-examine the two 138-vert "feet" `0x135050`/`0x1345ac`: 138 verts is large for a
foot -- check against MAME geo_parse whether they are the full lower leg decoded
flat, vs. genuinely flat feet.

gcc and clang build clean and produce byte-identical OBJs (Akira 31, Lau 23);
vf2_geo_display + the geometry differential tests pass. No capture/texture/OBJ data
committed.
