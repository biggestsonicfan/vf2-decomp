# tgp_microcode_no_texture_decode_v0830 — the TGP does not decode textures/UVs

**celsowm Option 2 go/no-go (one-session check).** Before committing to
recovering the polygon→texture link from the MB86234 TGP microcode, verify
whether the microcode actually reads/transforms/produces texture data (texture
headers, UVs) or only matrices and vertices. Result: **it does not touch
textures** — the UV / texture-header decode is in the video-board *geometrizer*,
not the TGP. Evidence below. This reopens the choice with celsowm (Option 1).

All evidence is reference-free against the committed ROM (`roms/vf2`) plus the
local-only fight capture (`..\snap\`, never committed). Analysis ran from the
scratchpad; no ROM/snaps/derived blobs are committed.

## Inputs

- `..\snap\vf2_tgp_program_ram_4096w.bin` — the MB86234 program RAM after boot
  (951 used words, rest zero fill; FIFO-uploaded at frame ~11, see
  `model_extractor_v0829.md`).
- `..\snap\vf2_fight_geometry_f1530_3frames.txt` — every i960 write to geo
  `0x800000`, prg `0x804000`, copro FIFO `0x884000`, ram9 `0x900000`, and every
  FIFO read-back (`fifoR`), 1P fight.
- `tools/python/tgp_disasm_mame.py` — the repo's analysis-only MB86233
  disassembler (MAME-inspired, BSD-3-Clause reference encoding).
- `poly.bin` / `main_data.bin` — regions rebuilt from the ROM in the scratchpad.

## Evidence 1 — the fighters' texture descriptors never reach the TGP [MEASURED]

Per-polygon texture/material data lives in the model record's descriptor words
(the attribute word of each face; low 16 bits = link/primitive code
`0x15xx/0x16xx/0x17xx/0x1a01`, high 16 bits = the texture/material field, e.g.
`0x98c0`, `0x99c0`, `0x8140`, `0x317c`). I took the exact descriptor words from
the P1 character records (object ids `0x494..0x4bd`, poly `0x3c1fac+`) and
searched every stream of the capture for them:

- **0 hits in any i960-written stream** (fifo, prg, geo, ram9).

The i960 only writes the object **pointer** (`w0`) to geo `0x00800010`; the
geometrizer then fetches that model — vertices *and* descriptors — straight from
the `copro_data` polygon ROM. So the character models' texture fields are never
streamed by the i960 at all, and in particular never enter the copro FIFO
(`0x884000`) that feeds the TGP. The TGP cannot decode what it never receives.

Routing of *descriptor-shaped* words generally (low16 a link code, high16 the
texture field, not a float), by stream:

| stream | addr | role | descriptor-shaped hits |
| --- | --- | --- | --- |
| prg | `0x804000` | geo payload → bufferram (geometrizer input) | 80 |
| fifo | `0x884000` | copro math FIFO (TGP) | 10 |
| geo | `0x800000` | geo cmd / object pointers | 0 |
| ram9 | `0x900000` | bufferram direct | 0 |

The only descriptors the TGP sees are those of **inline** `op-0x05` geometry
(`0x01ac1702`, `0x00ac1502`, `0x012c1602`), which the i960 streams to *both* the
geo port (prg, 80×) and the copro FIFO (10×). Those ride along the vertex stream
the TGP transforms; Evidence 2 shows the TGP does not extract a texture field
from them.

## Evidence 2 — the microcode is a geometry transform DSP, no texture decode [MEASURED]

Instruction-group histogram over the 951 used words (`tgp_disasm_mame.py`
encoding; group = `(opc>>26)&0x3f`):

| group | kind | count |
| --- | --- | --- |
| `0x07` | `ld/mov` mem↔mem (+ALU) | 547 |
| `0x00` | `lab/mov` mem (+ALU) | 119 |
| `0x2f` | branch / ldif / rtif (dispatch) | 111 |
| `0x0f` | rep / clr / set (loop control) | 106 |
| `0x10–0x1f` | load-immediate → register | 48 |
| `0x0e` | lipl/lia/lib/lid | 18 |
| other | | 2 |

ALU field usage inside the mem/move groups: **177 float ops**
(`fadd/fsbd/fml/fmsd/fmrd/fdvd/…` — the matrix×vector + perspective math) versus
**33 integer ops, only 7 of them shift/mask/logic** (`andd/lsrd/lsld/asrd/eord`)
in the entire program. Load-immediate constants are addresses/counts
(`0x200` = the `0x200–0x3ff` RAM bank base, `0x20`, `0x2c`, `0x2f6`, `0x7f`) —
**no texture-geometry constants** (no `0x7ff`/`0x800` sheet size, no `0x1f/0x3f`
page masks, no `0xffff`).

Decoding a Model 2 texture header per polygon (sheet x/y, width/height,
wrap/mirror flags, LOD, then per-vertex UV scaling) needs substantial integer
bit-extraction — many masks/shifts per face. Seven shift/mask ops total, no
size/mask constants, and 177 float ops is the signature of **transform +
projection + lighting**, i.e. a pure geometry engine with a function-dispatch
table at the top (`0x0000`–`0x002b` is a table of `br` entries).

## Conclusion [MEASURED]

**The TGP microcode does not read, transform, or produce texture headers or
UVs.** Therefore the polygon→texture decode (sheet rectangle + UVs) is performed
by the **geometrizer circuit of the video board**, downstream of the TGP, when
it rasterizes the models it fetches from the polygon ROM — not by anything the
VF2 ROM set runs on the MB86234.

This matches the MAME architecture split exactly: the MB86234 core
(`third_party/mame-mb86233/`, in-tree, BSD-3-Clause) does geometry math, while
`geo_parse`, the texture-header unpack and the textured-scanline draw live in
the **video/renderer** files `model2_v.cpp` + `model2rd.ipp` (the geometrizer),
which are **not** in-tree. So Option 2 (recover the decode from the TGP
microcode) has no target: the microcode isn't where the decode is. The decode
must be recovered from the geometrizer behaviour instead.

## License homework for the geometrizer path (celsowm asked) [VERIFIED]

If we pursue the decode via the geometrizer, the reference is the two MAME
renderer files. Their license headers, read directly from MAME `mame0289`
`src/mame/sega/`:

- `model2_v.cpp` → `// license:BSD-3-Clause` /
  `// copyright-holders:R. Belmont, Olivier Galibert, ElSemi, Angelo Salese, Matthew Daniels`
  (defines `model2_state::geo_parse`, `model2_3d_process_polygon` which reads the
  per-vertex U/V and the 4-word texture header, and `model2_renderer::model2_3d_render`
  which unpacks sheet position, width/height, wrap/mirror, microtexture; U/V are
  13.3 fixed-point, scaled by 1/8).
- `model2rd.ipp` → `// license:BSD-3-Clause` / same copyright-holders
  (the solid/textured scanline draw, bilinear/mip/microtexture sampling, luma +
  gamma colour path).

These carry the **same BSD-3-Clause header and the same copyright-holders** as
the Model 2 driver already in-tree (`third_party/mame-model2-ref/model2.cpp`,
`model2.h`) and as the in-tree MB86233 core. Repo policy
(`THIRD_PARTY.md`, `third_party/README.md`): MAME is **reference-only**,
BSD-3-Clause, **excluded from the CMake build**, never linked into
`src/recovered/`, the executor, or the portable C17 runtime — the flow is
"MAME suggests → our oracle measures on the VF2 ROM → differential proves".

So adding `model2_v.cpp` + `model2rd.ipp` to `third_party/mame-model2-ref/` as
navigation-only reference is **consistent in licence and policy** with the
existing `model2.cpp`/`model2.h` and `mame-mb86233/` precedent. It remains
**celsowm's call** (per the Option-1 decision), and this note is the evidence
for that conversation.

## Recommendation

Revisit with celsowm. Option 2 is a dead end (no microcode target). The
realistic route to the textured head is **Option 1**: add the two BSD-3-Clause
renderer files as reference, and recover the geometrizer's
descriptor → (sheet rect, per-vertex UV) decode clean-room, validated against
the VF2 polygon ROM and the fight capture as the oracle.

Independently of that decision, the **texel sheet format is already confirmed
bit-exact**: decoding a 2 MiB texture bank as `word = low16|high16`, each 16-bit
half four 4-bit texels MSB-nibble-first, 2048×2048, grayscale `index*17`,
reproduces the user's validated sheet PNG with **0 mismatches over 113,360
sampled texels** (`vf2_fight_tex0_f1808.bin` vs `vf2_fight_tex0_sheet.png`). That
decoder can be committed as CI-safe tooling whenever wanted; it does not depend
on the geometrizer question. See `texture_rom_format_v0827.md`,
`model_extractor_v0829.md`.
