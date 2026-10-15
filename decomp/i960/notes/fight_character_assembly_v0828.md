# fight_character_assembly_v0828 — character assembly from the MAME fight capture

**Slice 3 of the model-extraction plan.** How a character is assembled from
polygon-ROM pieces and placed, recovered from a MAME capture of an in-progress
fight (phase `0x50002a=09`). The capture is **local reference only, never
committed** (`..\snap\vf2_fight_geometry_f1530_3frames.txt` and the `.bin`
companions; 3 frames, every i960 write to geo `0x800000-0x803fff`, prg `0x804000`,
fifo `0x884000`, ram9 `0x900000-0x91ffff`, and fifo reads).

Tagging as in slices 1–2: **[MEASURED]** = verified against the capture and/or the
committed ROM; **[HYPOTHESIS]** = inferred, fail-closed. Evidence scripts ran from
the scratchpad; nothing derived was committed.

---

## 0. Character identity — UNCONFIRMED from the HUD

The capture is a 1P game started with coin+start+punch on the **default select
cursor** (Akira expected, per the capture header, unverified). The requested HUD
check does **not** resolve it: rendering the captured tile RAM
(`vf2_fight_tileram_f1536.bin` + palette) through `vf2_tile_render_layer` produces
glyph-soup, because the on-screen HUD character names are **graphical tiles**
(name-table banks 28/29/… — `entry>>8`), not ASCII font tiles. The font-glyph
decoder (`VF2_TILE_FONT_BASE`) only renders bank-0 ASCII, so the names are
unreadable as text in-tree. Identity is therefore **P1 = default-cursor character
(Akira-expected)**, to be confirmed visually by the slice-4 viewer (or by a
separate means if the maintainer has one).

## 1. Scene composition [MEASURED]

Per frame: **108 object submissions** (geo writes to `0x00801008`), 108 distinct
display-list addresses; ~2500 fifo words (~1400 command headers), ~1190 prg writes,
~400 geo writes, ~1600 fifo reads. The scene is **double-buffered**: the object
display-list base advances `+0x10000` each frame (f1 `0x86ec…`, f2 `0x106ec…`,
f3 `0x186ec…`), same low pattern.

Object-submission protocol (matches the recovered helper `0x7c60`,
`packet_format_p1_v0377.md`): per piece the i960 writes the display-list address to
geo `0x00801008` and the model word to geo `0x00800010`.

## 2. Piece → polygon-ROM model record [MEASURED — verified, extends slice 1]

The model word written to geo `0x00800010` resolves to a model record at
**`poly_byte = (word & 0x7fffff) * 4`** (the slice-1 word-index convention). Verified
reference-free: e.g. `0x001b444c → byte 0x06d1130` decodes as a clean slice-1
record — IEEE-LE vertex triples, descriptor words (`0x01f41b01`, `0xb1741a01`, …,
low-3-byte group descriptor + i960-reset top byte), skip-3 normals
(`0xbf800000` = −1.0), `0x00040401` end. Every tested model word lands on a
word-aligned record. So **each on-screen piece is a polygon-ROM model record at
`(geo_0x800010 & 0x7fffff)*4`** — this is the concrete hook the extractor uses.

### Character clusters [MEASURED]
Mapping all 108 pieces (f1) to poly bytes and grouping by contiguity:

| cluster | poly byte range | ~pieces | role |
| --- | --- | ---: | --- |
| **P1 (submitted first)** | `0x06d1130 .. 0x06d7468+` | ~26 | first character (Akira-expected) |
| P2 | `0x02c9000 .. 0x02f1000` | ~16 | opponent |
| misc | `0x0257318`, `0x027e228`, `0x0337238`, `0x12a3c50`, `0x13a32d0`, `0x1224508`, … | rest | stage / shared / accessories |

So a **character = a contiguous run of ~16–26 model-record pieces** (body parts) in
one polygon-ROM region; P1's pieces live at **poly `0x6d1130`+**. This directly
answers "which pieces compose the character": the extractor reads the P1 cluster's
model words from the capture and decodes each record via slice 1.

## 3. Placement: matrices + hierarchy [MEASURED mechanism]

- **World/root transforms:** exactly **2 class-0x0b matrices per frame** in the FIFO
  (opcode `(w>>23)&0x7f == 0x0b`, 12 payload words = a **3×3 rotation + 3-vector
  translation**), e.g. f1 matrix[0] rot≈[[-0.92,0.38,0.06],[0.37,0.91,-0.19],
  [-0.13,-0.16,-0.98]] t≈(-4.39,1.22,1.35). Both matrices **change every frame**
  (animation) — they are the **two fighters' root/world placements**.
- **Per-joint transforms are TGP-side, not in the i960 FIFO:** only 2 class-0x0b
  blocks exist for 108 pieces, so per-piece posing is computed by the TGP/geo
  processor from skeleton data, not emitted as FIFO matrices. This matches the
  standing "the TGP does the transform" finding (G2). The capture's **prg stream
  (~1190 writes)** is the *transformed vertex output* (model records after the TGP
  applies the pose) plus the geo palette-pack words (`0x7f`, `0x1f001ff`,
  `0xf8013f` from helper `0x7f24`).
- **Hierarchy [HYPOTHESIS]:** the submission order (display-list sequence) groups a
  character's pieces contiguously; the per-joint parent/child skeleton is not in
  this capture (it is TGP-side). Pinning the skeleton needs the TGP transform input
  or a reference pose dump.

## 4. Descriptor → texture link — STILL UNPROVEN

Tested `(descriptor >> 16) & 0x7f` as the texture number (the measured valid range
is `0..0x56`, `texture_number_diagnostic_v0196.md`): it fits the *transformed* prg
descriptors broadly (255–344 / 347 in range) but **fails on raw poly descriptors**
(e.g. `0x01f41b01 → 0x74`, out of range), so it is **not** the texture id. No clean
descriptor subfield maps to `0..0x56` across both raw and transformed records. The
3D texture RAM (where the id would resolve to pixels) is **not in this capture**
(only tile RAM + palette), so the link cannot be closed here. Remains the open gap
from slice 2.

## 5. What the extractor (slice 4) can do now, and what it still needs
- **Can:** extract P1's mesh — read the P1-cluster model words from the capture (or
  the geo `0x800010` stream), decode each record via slice 1 into vertices + faces,
  and emit OBJ. Untextured geometry is fully reachable.
- **Still needs (for texture + exact pose):** the descriptor→texture id mapping and
  the 3D texture-RAM upload (a texture-RAM capture or the g3-source trace from slice
  2), and the TGP skeleton/joint transforms for the exact animated pose. The world
  matrices (§3) place the character as a whole.

## 6. Ask for the maintainer
- HUD-text identity confirmation is not possible in-tree (graphical tiles). If a
  definite "this is Akira" is needed before slice 4, a confirmation outside the tile
  layer would help; otherwise the slice-4 viewer will show the mesh for visual
  confirmation.
- To close textures: a capture of the **3D texture RAM** (`0x12000000`/`0x12400000`
  banks) for this fight, or confirmation of where the texture decoder's g3 source
  reads from.
