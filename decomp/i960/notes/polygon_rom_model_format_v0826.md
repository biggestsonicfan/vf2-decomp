# polygon_rom_model_format_v0826 — full model format in the polygon ROM

**Slice 1 of the model-extraction plan** (celsowm priority: fill the TGP/graphics
gaps enough to feed a 3D-model extractor). Continues `b2132cf2`
("recover the polygon-ROM model record structure"), consolidating the scattered
findings of `packet_format_p1_v0377.md` and `mame_3d_frame_geometry_format.md`
into one format description: **model table → model record → vertices / polygons /
normals / attributes**.

Everything tagged **[MEASURED]** is verified reference-free against the committed
ROM set (`roms/vf2`, gitignored; CI builds the regions from it). Everything tagged
**[HYPOTHESIS]** is a decode guess not yet oracle-pinned at a geometry
vertex-write boundary — do **not** promote those to recovered C (AGENTS.md rule 5;
see `packet_format_p1_v0377.md` §8 for the fail-closed boundary and promotion
procedure).

Evidence this pass: polygon region rebuilt from the ROM files; the pinned 13-word
verbatim copy at byte `0x1bb870` reproduced exactly; records walked under the
word-index convention (below). All evidence scripts ran from the scratchpad; no
ROM/snaps/derived blobs are committed.

---

## 1. Model (object) table — in the main program ROM [MEASURED]

- **Location:** base `0x020e0004` in the maincpu image (`main_data`), indexed by
  object id.
- **Stride:** 16 bytes = 4 little-endian words per entry `{w0, w1, w2, w3}`
  (monotonic w0/w2 across consecutive ids confirms the 16-byte stride;
  cf. `packet_format_p1_v0377.md` §4).

| word | role | tag |
| --- | --- | --- |
| `w0` | geo-port header word; i960 stores it to geo `0x00800010` **and** `0x00804000` | [MEASURED] (`packet_format` §3.1/§4) |
| `w1` | geo-port word; i960 stores it to geo `0x00804004` | [MEASURED] |
| `w2` | **TGP geo address of the model record** (see §2) | [MEASURED] |
| `w3` | size/count pair; pol_test family `w3 = ((n-1)<<16) \| n` (n = primitive count). Attract ids use a different, unproven encoding. | [MEASURED family] / [UNPROVEN attract] |

## 2. w2 → polygon-ROM offset: the address is a WORD index [MEASURED — pinned this pass]

`w2` is a TGP data-space address (MAME maps `copro_data` ROM at
`0x00800000`–`0x009fffff`, word-addressed):

```
bank        = (w2 >> 23) & 1      ; 0x00800000 bit = polygon-ROM bank select
word_index  =  w2 & 0x7fffff      ; index in 32-bit WORDS, not bytes
byte_offset =  word_index * 4     ; offset into the little-endian polygon region
```

**This resolves a standing ambiguity and a tool bug.** `packet_format_p1_v0377.md`
§5 already tagged `w2 & 0x7fffff` as a *word_index* [MEASURED], but
`tools/python/dump_poly_objects.py` reads it as a **byte** offset (no `*4`). For
the `0x9xx` ids that lands on a misaligned address and reads non-float garbage
(e.g. id `0x97d`: byte `0x18a0b1`, `&3==1` → `0x99be32fe`). With the `*4` word
convention, id `0x97d` resolves to byte `0x6282c4` (aligned) and reads
`0xbe4ccccd 0x00000000 0x3e4ccccd` = **(−0.2, 0, +0.2)** — exactly the vertex the
independent `packet_format` §3.3 decode predicted. **The extraction tool (slice 3)
must use the `*4` word convention**, and `dump_poly_objects.py` needs the `*4`
fix.

Verified this pass by walking records to their `0x00040401` terminator under the
word convention (all land on aligned float/descriptor data; spans match the
`packet_format` §4 w3-span family):

| id | w2 | byte offset | record shape to `0x00040401` |
| --- | --- | --- | --- |
| `0x97d` | `0x0098a0b1` | `0x6282c4` | verts + desc `0xe1001601` @w6 + normal (0,−1,0) @w7–9 + verts; END @w17; w3 n=2 |
| `0x985` | `0x0098a9c9` | `0x62a724` | same shape; desc `0xe1021601`; END @w17; w3 n=2 |
| `0x140` | `0x0083f17c` | `0xfc5f0`  | large mesh; descriptors `0xc9901701 / 0xc8901501 / 0xc8901701` every ~10 words; w3 n=68 |
| `0x148` | `0x00840430` | `0x1010c0` | large mesh; descriptors `0xd0a81502 / 0xd1281601`; matches the attr words `packet_format` §9 cited |

## 3. Model record format (at `byte_offset`) [MEASURED framing / HYPOTHESIS semantics]

A record is a stream interleaving vertex groups with descriptor words, ending in a
terminator + zero pad:

```
[vertex group] [descriptor] [vertex group] [descriptor] ... [0x00040401 END] [0x00000000 pad...]
```

### 3.1 Vertices [MEASURED]
IEEE-754 little-endian `float` triples `(x, y, z)` in model space (magnitudes
≲ 1; e.g. ±0.2). **Vertices repeat** — the same triple appears in adjacent groups,
i.e. a vertex shared between neighbouring faces / strip entries (pinned
reference-free: the triple `3d0ce704 bc83e426 3db43958` appears twice in a row in
the `0x1bb870` record; `mame_3d_frame_geometry_format.md`).

### 3.2 Descriptor / attribute words [MEASURED encoding / UNPROVEN high bits]
Non-float words interleaved between vertex groups. Low 3 bytes = a group/type
descriptor; top byte = flags.

- Observed low-3-byte descriptors: `0x101602`, `0x901502`, `0x981501`,
  `0xa81502`, `0x281601`, `0x901701`, `0x021601`, `0x001601`. [MEASURED]
- **Top-byte flag is rewritten by the i960** when the record is handed to the geo
  port: poly `0x1bb86c = 0xf9101602` becomes `prg[132] = 0x01101602` — low 3 bytes
  `0x101602` preserved, top byte `0xf9 → 0x01/0x00`. [MEASURED — b2132cf2, via the
  prg↔poly alignment]
- High bits (texture / colour / material split of e.g. `0xe1001601`, `0xd0a81502`,
  `0xd1281601`) — **[UNPROVEN]** (`packet_format` §9).
- `(attr & 3) == 0` as an in-record face-list/end token — **[HYPOTHESIS]** from
  `src/hardware/tgp.c`.

### 3.3 Normals (skip-3 slot) [HYPOTHESIS]
The triple immediately following a descriptor, present when
`geometry_mode & 3 < 2` (the "skip3" slot). On the test models `0x97d`/`0x985` it
reads `(0, −1, 0)` (word `0xbf800000` = −1.0) — a unit normal. Normal-like on the
test models, **unproven** for records generally (`packet_format` §5/§9).

### 3.4 End marker + pad [MEASURED]
The record ends with the attribute word `0x00040401`, followed by zero-word
padding up to the next record. (Confirmed at byte `0x1bb894` and at the
`0x97d`/`0x985` record ends, word 17.)

## 4. What is pinned vs what still needs the oracle

**Pinned reference-free (against the committed polygon ROM):**
- model-table location, 16-byte stride, and the `{w0,w1,w2,w3}` word roles;
- the `w2` **word-index** convention (`byte = (w2 & 0x7fffff) * 4`) and bank bit;
- record framing (vertex groups + descriptors + `0x00040401` end + zero pad);
- vertex encoding (LE IEEE floats, shared-vertex repeats);
- descriptor low-3-byte values and the i960 top-byte reset.

**Still needs the reference (a MAME TGP/geo vertex-write boundary or triangle-list
output):**
- descriptor high-bit split (triangle vs strip, texture id, colour/material);
- the skip-3 slot semantics (normal vs light vs unused) in general;
- the attract-id `w3` size/count encoding;
- per-record vertex/triangle counts for dense models (the walk gives framing, not
  a verified primitive count).

The slice-4 validation (extracted vertices vs the `prg` stream of
`snap/vf2_attract_frame_geometry_f1000.txt`, out of repo) is the oracle that can
promote the [HYPOTHESIS] items; until then the extractor (slice 3) must emit the
[MEASURED] geometry and keep the unproven fields clearly flagged.
