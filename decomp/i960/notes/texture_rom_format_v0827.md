# texture_rom_format_v0827 — texture ROM layout, texel/palette format, polygon→texture link

**Slice 2 of the model-extraction plan** (celsowm priority). Consolidates the
texture format for the 3D-model extractor: **texture ROM layout → runtime decode
pipeline → on-GPU texel/palette/luma format → polygon→texture link**, drawing the
recovered-C facts together (`texture_bridge_texture.c` + the `texture_*_v0024.md`
notes) and pinning the ROM-side facts reference-free against the committed texture
ROM.

Tagging as in slice 1 (`polygon_rom_model_format_v0826.md`): **[MEASURED]** =
ROM-backed differential MATCH or read directly from the committed ROM;
**[HYPOTHESIS]** = inferred, not oracle-pinned — do not promote to recovered C
(AGENTS.md rule 5). Evidence scripts ran from the scratchpad; no ROM/snaps/derived
blobs committed.

---

## 1. Texture ROM physical layout [MEASURED — verified reference-free]

`VF2_REGION_TEXTURES` (`rom_manifest.c:8`): **16 MiB** (`0x01000000`), fill `0xff`.
Built from four 2 MiB mask ROMs as 32-bit word-interleaved low/high pairs
(`rom_manifest.c:35-38`):

| ROM | region offset | role |
| --- | --- | --- |
| `mpr-17553.25` | `0x00000000` (even) | texture pair 0 low |
| `mpr-17552.24` | `0x00000002` (odd)  | texture pair 0 high |
| `mpr-17547.27` | `0x00800000` (even) | texture pair 1 low |
| `mpr-17546.26` | `0x00800002` (odd)  | texture pair 1 high |

Interleave: region `u32 = low_rom[2k..2k+1] | (high_rom[2k..2k+1] << 16)`. Verified
by rebuilding the region from the ROM files:

- Populated data occupies **two 4 MiB halves**: `0x000000–0x400000` (pair 0) and
  `0x800000–0xc00000` (pair 1); `0x400000–0x800000` and `0xc00000–0x1000000` are
  `0xff` fill. ≈8 MiB of texture data total.
- Per-1 MiB Shannon entropy is **2.5–4.8** (not ~8.0), and the data contains
  **verbatim repeated 32-bit word patterns** (e.g. `0x00006001 0x0c001b04` ×4 at
  the region start). Both facts say the ROM data is **structured / not
  high-entropy-compressed**.

**Decode target is separate texture RAM, not this ROM** (`model2a.h:76-79`):
`VF2_TEXTURE_RAM0_BASE=0x12000000`, `RAM1_BASE=0x12400000`, two 2 MiB banks + 2 MiB
mirror.

## 2. Runtime texture decode pipeline [MEASURED — recovered C, ROM differential MATCH]

`src/recovered/texture_bridge_texture.c` + `texture_header_decode_v0024.md`,
`texture_tree_dispatch_v0024.md`, `texture_color_prepare_v0024.md`. The i960
decodes a **little-endian bit stream** (LSB-first 16-bit reservoir reader,
`texture_bit_reader_*`, lines 38-108) into texture RAM.

### 2.1 Header (bitstream, read from the source pointer in g3 = `registers[19]`)
After `initialize` + two `refill`s the accumulator holds the first two LE 16-bit
words; fields are then taken LSB-first (`:3150-3172`):

| field | bits | note |
| --- | --- | --- |
| raw_width  | 8 | `width = (raw_width+1)>>1` |
| raw_height | 8 | `height = (raw_height+1)>>1` |
| code_bits  | 8 | gate: 1..31 |
| symbol_count | 16 | gate: 1..`0x100000` |
| table_a | 16 | |
| table_b | 16 | |
| nibble | 4 | |
| table_c | 16 | |

So at a source address `A`: `raw_width=byte[A]`, `raw_height=byte[A+1]`,
`code_bits=byte[A+2]`, then LSB-packed 16-bit fields. 36-byte decoded header
written to `VF2_TEXTURE_HEADER_OUTPUT=0x0055c320`. The decoder is a **Huffman/tree
+ byte/word RLE** family (`0x0004c3f0` symbol table, `0x0004c4d4` pair table,
`0x0004c544`/`0x0004c928` 256-entry tree expander, `0x0004c6e0`/`0x0004c868`
byte decode/run, `0x0004cc28`/`0x0004cce8` word decode/run, `0x0004ce88` convert).

### 2.2 Texel/palette/luma output format (`execute_texture_convert`, `0x0004ce88`, `:2927`)
Per texel the convert stage emits **two planes**:

- **16-bit colour sample** = `current | (current >> 12)` (`:3005`), stored u16 with
  a 4-byte stride (`destination += 4`). [MEASURED mechanism]
- **4-bit luma/intensity nibble** = `((mixed + (mixed>>8)) >> 2) & 0xF` where
  `mixed = current + (current>>16)` (`:3010-3018`), stored one byte to `odd_address`
  (`-=2` per texel). [MEASURED]
- **even/odd plane swizzle**: `odd_address`/`even_address` swap every row
  (`:3035-3040`); bases `0x0055c2ef`/`0x0055c2ee` — an interleaved two-row luma
  plane. [MEASURED]
- **RLE**: `current == previous` repeats the prior colour+nibble (`:2988`).
- source read **backwards** in 32-bit words from `0x0055c2ec` (`:2986`); bounds
  rows/cols 1..1024; `bytes_written = pixels*3` (2 colour + 1 nibble).

**Colour-field meaning:** the 16-bit sample is packed and stored with no palette
lookup in the convert function. The 2D tile layer uses RGB555
`[x RRRRR GGGGG BBBBB]` with 5→8 expansion (`tile.c:75` `vf2_tile_palette_rgb555`);
**[HYPOTHESIS]** the texture 16-bit field is the same RGB555, but this is not
asserted in-tree.

## 3. Polygon → texture link [UNPROVEN — the real gap]

Not recovered. `src/hardware/tgp.c` parses only the polygon attribute **low** bits
(`:830-901`): `attr&3==0` end, `attr&1` quad/tri, `(attr>>8)&3` strip-link mode,
`geometry_mode&3<2` skip-3 — all tagged "hypothesis from tgp.c" in
`packet_format_p1_v0377.md` §5. The **texture/colour high bits** of the descriptor
words recovered in slice 1 (`0xe1001601`, `0xd0a81502`, `0xd1281601`, `0x01281601`,
…) are **unproven** (`packet_format` §9); colour is passed to
`geometry_render_polygon` as a separate parameter with no texture-id extraction.

Only proven texture-id semantics: a **valid range 0..0x56** — the record publisher
at `0x0004b9b8` renders `tex num error` into tile RAM for numbers `> 0x56`
(`texture_number_diagnostic_v0196.md`). Three texture counters at
`0x005502c0/d0/e0` drive publish/expiry (`texture_counter_update_v0024.md`).

**To pin the link:** a MAME capture of a frame with a textured character on screen
(the slice-3 "Akira pieces + matrices" capture) giving the descriptor word ↔
texture-id ↔ UV correspondence, diffed against the model records. Until then the
extractor emits geometry untextured (or with a provisional id = a chosen
descriptor subfield, clearly flagged).

## 4. Texture ROM organisation: raw vs compressed [open — evidence narrows it]

The runtime decoder (§2) reads its compressed bitstream from a **work-RAM pointer
(g3)**, a DMA-staged blob — it does **not** index the 16 MiB texture ROM directly
in any recovered path. So how the ROM maps to textures is not pinned. Evidence
gathered this pass:

- ROM `0x0` = `00 80 00 …` → `code_bits=0`, fails the §2.1 gate; `0x800000` likewise
  fails. Neither populated half **starts** with a valid compressed header.
- Scanning both populated halves at word boundaries, **14.5%** of offsets pass the
  (weak) header gate, with repeated spurious dims (`code_bits=1`) — i.e. there is
  **no clean grid of real headers**; the ROM is **not** a flat concatenation of
  headered compressed streams at findable offsets.
- With the low entropy + verbatim word repeats (§1), the texture ROM is most
  consistent with **raw/structured texel data** (or compressed blocks reached via
  an unrecovered directory + DMA), not a bitstream the §2 decoder consumes
  verbatim from ROM.

**Still needed for the extractor:** a runtime trace of what the g3 source actually
points at when `execute_texture_header_decode` runs (does it DMA from the texture
ROM, and from which offset?), plus a texture directory/table if one exists. This,
or the slice-3 MAME capture, resolves both the ROM→texel mapping and the
polygon→texture link.

## 5. Tooling [MEASURED]

- `vf2i960 compare-texture-bridge <rom-dir>` (`commands.c:67,7672`) — ROM-backed
  differential replay of the recovered texture bridge (the only dedicated texture
  CLI). No raw-ROM texture→PNG decoder exists yet (that is slice 4's exporter).
- `vf2i960 trace-orchestrator` — read-only CSV trace of `[0x0004bb18,0x0004c180]`.
