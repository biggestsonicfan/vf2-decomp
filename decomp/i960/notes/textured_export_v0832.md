# textured_export_v0832 — player-1 textured OBJ + MTL + PNG

**celsowm Option 1, slice 3a** ("resolve the blocker until the very end"):
export the player-1 textured mesh. Builds on the recovered link
(`texture_polygon_link_v0831.md`). Output is **local only, never committed**;
the decode is covered synthetically in CI (`test_texture_link.c`).

## Command

`vf2i960 export-textured <rom-dir> <capture.txt> <tex0.bin> <tex1.bin> <out-dir>`

- `<capture.txt>`: the fight geometry capture (geo `0x800010` object words).
- `<tex0.bin>/<tex1.bin>`: the two texture-RAM bank captures (2 MiB each).
- Writes `player1.obj`, `player1.mtl`, `tex0.png`, `tex1.png` to `<out-dir>`.

Player 1 = the **largest record-contiguous cluster** of on-screen objects
(a fighter has far more pieces than stage/shadow objects; ties keep the
lowest-address one, which is P1). On the committed ROM + capture this resolves
to poly **`0x3c1fac..0x3ca730`** — the Akira range pinned in
`fight_character_assembly_v0828.md`/`model_extractor_v0829.md` — 29 pieces,
524 triangles.

## UV ↔ vertex pairing [MEASURED — geometrizer-exact]

Per primitive of the `[attr][normal][vA][vB]` quad strip, the texture-point
table (at the object's w0) yields `NumVerts` fresh `(pv,pu)` 13.3-fixed pairs in
the order `[P1(n-1), P0(n-1), P0(n), P1(n)]`. With `p0=P0(n-1)`, `p1=P1(n-1)`,
`vA=P0(n)`, `vB=P1(n)`, the corner→UV map is `p0←uv[1]`, `p1←uv[0]`, `vA←uv[2]`,
`vB←uv[3]`. The emitted triangles are `(p0,p1,vB)` and, for a quad, `(p0,vB,vA)`,
each vertex carrying its paired UV. The strip advances exactly as the hardware:
`(attr>>8)&3` 0/2 → `p0=vA,p1=vB`; 1 → `p1=vA` (= P0(n)); 3 → `p0=vB` (= P1(n)).

> This refines `model_decode.c`, whose strip sets `p1=vB` (case 1) / `p0=vA`
> (case 3) — a vA/vB swap that is harmless for triangles (vA==vB, the bulk of
> character faces) but wrong for case-1/3 quads. The textured exporter uses the
> hardware-exact advance.

Each UV becomes a sheet coordinate `(tex_x + (pu/8 mod tex_width), tex_y +
(pv/8 mod tex_height))` and is normalised over the logical 2048×1024 sheet
(OBJ V flipped). Faces are grouped by `usemtl sheet0`/`sheet1` per the header's
sheet bit.

## PNG [MEASURED]

`tex0.png`/`tex1.png` are the **de-swizzled logical 2048×1024** sheets (grayscale
`index*17`), via `vf2_texture_sheet_texel` (2×2 swizzle + the 2048×1024↔1024×2048
fold). Written by a small dependency-free encoder (8-bit grayscale, zlib
"stored"/uncompressed deflate, CRC via `vf2_crc32`, inline Adler-32). Validated:
the exported `tex1.png` byte-matches an independent de-swizzle of the bank
(0 mismatches sampled).

## Scope / what remains

- **Grayscale, not colour.** The 4-bit value is a luma index; the recognizable
  image (face, eyes, hair, cloth) is the grayscale sheet celsowm already
  confirmed. Full colour (luma RAM 0x12800000 + palette) is a later refinement.
- **Not posed.** The pieces sit in per-part model-local spaces (extents ±0.2 to
  ±96), so the combined OBJ overlays them rather than standing Akira up; each
  piece is individually correct (geometry + UVs + texture). Posing (per-joint
  TGP transforms) is a separate step.

## Next (slice 3b)

Display the textured player-1 mesh in the SDL3 viewer via **SDL_GPU** (not raw
Vulkan): upload the sheet PNG as a texture, sample it in a fragment shader.
