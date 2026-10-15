# model_extractor_v0829 — OBJ exporter + SDL3 viewer (slice 4, untextured)

**Slice 4 of the model-extraction plan.** Committed, CI-safe tooling to extract and
view a character's mesh from the polygon ROM, driven by a MAME fight capture.
Untextured (textures are a later slice). Outputs are **local only, never
committed**; CI tests use **synthetic data only**.

## New committed code
- `src/platform/model_decode.c` / `include/vf2/model_decode.h` — the model-record
  decoder (in `vf2_core`). `vf2_model_decode_record` mirrors `src/hardware/tgp.c`
  `geometry_execute_object` (read p0,p1; per link: attribute, optional 3-word
  normal, p2, optional quad p3; strip-advance by `(attr>>8)&3`), and
  `vf2_model_lookup_w0` resolves an object word to a record offset via the object
  table. **Validated reference-free**: the pol_test record `0x97d` decodes to its
  exact 2-triangle ±0.2 quad.
- `tests/platform/test_model_decode.c` — synthetic unit test (CI, no ROM).
- `vf2i960 extract-models <rom-dir> <capture.txt> <out-dir>` — parses the capture,
  resolves each piece, decodes it, clusters by polygon-ROM region, and writes one
  `playerN.obj` per character plus `scene.obj`.
- `tools/vf2_model_viewer/main.c` — SDL3 viewer (`VF2_BUILD_VIEWER=ON`,
  default OFF, shares the `vf2_host` SDL3 setup): loads an OBJ, software-projects
  and flat-shades via `SDL_RenderGeometry`, drag/arrow rotate, wheel/`+-` zoom;
  `--selftest` runs headless on the dummy driver (CI).

## The correct extraction pipeline [MEASURED — corrects slice 3]
For each on-screen piece:
1. the i960 writes an object word to geo `0x00800010` (captured) — this is the
   object table's **w0**, *not* the polygon pointer;
2. find the object-table entry (`0x020e0004`, 16-byte `{w0,w1,w2,w3}`) whose w0
   matches → read **w2**;
3. record byte offset = `(w2 & 0x7fffff) * 4`, bank = `(w2>>23)&1` (slice-1 word
   index);
4. decode the record (above).

**Correction to `fight_character_assembly_v0828.md` §2:** that note read the geo
`0x00800010` value *itself* as `(value & 0x7fffff)*4` and so landed at poly
`0x6d1130`, which is **not** a record start (word[2] is a descriptor). The value is
w0; the real record is reached through the table → w2 (e.g. id `0x494` → w2
`0x8f07eb` → poly byte `0x3c1fac`, which decodes as a clean record). The P1 cluster
is at poly **`0x3c1fac..0x3ca730`** (ids `0x494..0x4bd`, ~29 pieces); P2 at
`0x7804ac..0x780df4` (ids `0xce5..0xcf3`).

Record terminator: a model record ends at the measured `0x00040401` word (or an
`(attr&3)==0` zero attribute). tgp.c instead stops after a link count from the
object descriptor; a standalone extractor has no reliable count, so it breaks on
the terminator and fail-closes any vertex with `|coord| > 1e4` or non-finite
(a derailed strip reading a descriptor as a coordinate).

## Honest result: raw overlay is NOT a recognizable figure [MEASURED]
Extracting and rendering P1 (and the whole scene) does **not** yield a recognizable
Akira. The decoded pieces sit in **per-part model-local coordinate spaces** at
wildly different scales within one scene (observed piece extents range from ±0.2 to
±96), so overlaying them at a common origin produces a jumble/smear, not a posed
character (the first P1 piece, id `0x494`, is itself a flat ±96 shadow quad). The
decoder is correct per-record (0x97d validated); what is missing is **posing**:
each piece's joint transform.

Those per-joint transforms are **TGP-computed** (slice 3: only 2 FIFO class-0x0b
world matrices exist for ~108 pieces). They are produced by the **MB86234 TGP
microcode**, which is present in the game and loaded via the FIFO at frame 11
(captured as `..\snap\vf2_tgp_program_ram_4096w.bin`, 951 words). So the path to a
recognizable, confirmable Akira is: run/lift that microcode to pose the pieces,
then texture them (texture RAM captures `..\snap\vf2_fight_tex0/1/luma_f1808.bin`
are now in hand). See `polygon_rom_model_format_v0826.md`,
`texture_rom_format_v0827.md`, `fight_character_assembly_v0828.md`.

## Status
The exporter, decoder (+ synthetic test), and viewer are in and build green under
gcc and clang; the viewer will display the posed mesh once posing is applied. What
slice 4 does **not** do: assemble/pose a character, or texture it.
