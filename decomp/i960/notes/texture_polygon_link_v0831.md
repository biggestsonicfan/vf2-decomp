# texture_polygon_link_v0831 — the polygon→texture link, recovered

**celsowm Option 1 ("resolve the blocker until the very end").** The per-polygon
texture link — which part of a 2048×2048 sheet a face uses, and its per-vertex
UVs — is decoded by the **video-board geometrizer**, not the TGP
(`tgp_microcode_no_texture_decode_v0830.md`). This note recovers that decode
**clean-room** (own code/structs in `src/platform/texture_link.c`), using the
BSD-3-Clause MAME renderer added as reference in the previous commit
(`third_party/mame-model2-ref/model2_v.cpp`, `model2rd.ipp`) to learn the format,
and validates it against the committed VF2 ROM and the local fight capture. No
MAME code is copied into the build; `third_party/` stays reference-only.

## 1. Object submission = the object table [MEASURED]

An object is drawn by the geometrizer command "object data", four words. For VF2
these are exactly the object-table entry `{w0,w1,w2,w3}` at `0x020e0004`
(`polygon_rom_model_format_v0826.md`):

| word | geo name | meaning |
| --- | --- | --- |
| w0 | tpa | **texture-point (UV) table address** — u16 index into the texture region (or texture RAM if bit `0x800000`) |
| w1 | tha | **texture-header table address** — likewise |
| w2 | oba | polygon-record address — byte `= (w2 & 0x7fffff) * 4` |
| w3 | obc | primitive count |

This corrects `polygon_rom_model_format_v0826.md` §1, which tagged w0/w1 only as
"geo-port header words": **w0 and w1 are the UV and texture-header table
pointers.** The texture region is `VF2_REGION_TEXTURES` (the 16 MiB "textures"
ROM) addressed as u16 (`texture_rom_mask = bytes/2 - 1`).

## 2. Per-primitive walk [MEASURED]

The record is the quad-strip of `model_decode.h`: `p0,p1` then 10-word blocks
`[attr][normal xyz][vertexA xyz][vertexB xyz]`, ending at `0x00040401` /
`(attr&3)==0`. `NumVerts = (attr & 1) ? 4 : 3` (a triangle still occupies the vB
slot — the `vA==vB` padding seen in the records). For each primitive:

- **UVs:** read `NumVerts` `(pv,pu)` pairs from the UV table at the running UV
  pointer, then advance the pointer by `NumVerts*2` u16. Each component is
  **13.3 fixed-point texels** → divide by 8.
- **Texture header:** read 4 u16 at the running header pointer, then advance it
  by a **signed** 5-bit delta `((attr>>12)&0x1f)` × 4 u16 (observed `+1`×4 for
  character pieces = one fresh header per polygon).

(The attribute also carries, per the geometrizer: `(attr>>18)&0x1f` a
lighting/texture-parameter index, `attr>>27` a distance-coefficient index,
`(attr>>10)&3` the polygon z-sort mode. Not needed for the sheet/UV link.)

## 3. Texture-header bitfields [MEASURED]

Four u16 `th[0..3]`:

| field | bits | value |
| --- | --- | --- |
| tex_width | `th0 & 7` | `32 << n` (32..4096) |
| tex_height | `(th0>>3) & 7` | `32 << n` |
| wrap_x / wrap_y | `th0` bit 6 / 7 | smooth wrap (forced 0 if the axis mirrors) |
| mirror_x / mirror_y | `th0` bit 8 / 9 | |
| renderer | `(th0>>13) & 3` | bit0 translucent, bit1 textured |
| checker | `th0` bit 15 | |
| luma_base | `(th1 & 0xff) << 7` | |
| tex_x | `32 * (th2 & 0x3f)` | tile left on the logical sheet (0..2016) |
| tex_y | `32 * ((th2>>6) & 0x1f)` | tile top (0..992) |
| sheet | `th2` bit 12 | which texture bank (0/1) |
| micro-tex | `th0` bit 12 / bits 10-11, `th2` bits 13 / 14-15 | microtexture x/y/minlod |
| color_base | `(th3>>6) & 0x3ff` | solid/untextured colour index |

## 4. Sheet texel addressing [MEASURED]

The sheet is **logical 2048×1024** (the "upper half" the capture showed used),
stored **folded** as physical 1024×2048: for a logical `(x,y)`, if `x>=1024`
then `x-=1024; y^=1024`. Within that, texels are **2×2-swizzled**: a 16-bit half
of each 32-bit word holds a 2×2 block, `offset=(y/2)*512+(x/2)`, the word is
`bank[(offset>>1)]`, `offset&1` picks the high/low half, and `y&1`/`x&1` pick the
nibble (4-bit luma index). So the linear sheet decode of
`texture_sheet.c` (which matched the reference PNG) is visually right but not the
addressing UVs need; `vf2_texture_sheet_texel` applies the fold + swizzle.

A vertex's sheet pixel = `(tex_x + (pu/8 mod tex_width), tex_y + (pv/8 mod
tex_height))` (wrap) fed through that addressing; the 4-bit index becomes colour
via luma RAM + palette (a later slice).

## 5. Validation [MEASURED]

- **Against the ROM:** `vf2i960 texture-link roms/vf2 0x49b` (a P1 body mesh)
  decodes every primitive to a sane tile — `sheet1 256×256 @ (1024,0)` and
  `128×128 @ (1664,0)`, on the 32-texel grid, inside the 2048×1024 sheet — with
  per-vertex UVs in range (e.g. `(42.9,123.4)…(106.2,123.6)` for a 128-tile).
  Across the P1 pieces (`0x494..0x4bd`) the headers decode to valid power-of-two
  sizes and in-bounds origins; the fighters all target `sheet1`.
- **Synthetic CI** (`tests/platform/test_texture_link.c`, no ROM): header
  bitfields, signed header-pointer advance, 13.3→texel, and the swizzle/fold
  texel fetch (including the `x>=1024` fold).

## 6. New code (this commit)

- `src/platform/texture_link.c` / `include/vf2/texture_link.h` (in `vf2_core`):
  `vf2_texture_header_decode`, `vf2_texture_header_advance_words`,
  `vf2_texture_uv_texels`, `vf2_texture_sheet_texel`. Clean-room, fail-closed.
- `tests/platform/test_texture_link.c`: synthetic unit test.
- `vf2i960 texture-link <rom-dir> <object-id>`: dump an object's decoded link
  (ROM-only, local; not run by CI).

## 7. Next (slice 3)

Export the textured pieces (OBJ + MTL + PNG) — build the de-swizzled sheet
sub-image per tile and emit per-vertex UVs — and show the textured player-1 mesh
in the SDL3 viewer via SDL_GPU. Colour (luma RAM + palette) folds in there. See
`model_extractor_v0829.md`, `texture_rom_format_v0827.md`.
