# geo_pipeline_step4g_texheader_from_tha_v0865 — texheader/UV from the object command's tha/tpa (Lau's vest)

**Geometriser pipeline, Step 4 (colours), decode bug.** kazuyette, in Blender: Lau's vest
is navy with red speckles instead of red with a green dragon. Tracing showed the texel->
colour conversion is byte-faithful to MAME, so the wrong colour had to be a wrong texheader.

## Diagnosis [REFERENCE-CHECKED + MEASURED]

MAME `geo_object_data` (model2_v.cpp:1715-1728): the 0x01 object command is `{tpa, tha, oba,
obc}`. UV points are read straight from **tpa** (command word 0) and the texture header from
**tha** (command word 1), each selecting texture RAM (`addr & 0x800000` -> `addr & 0xffff`)
or texture ROM (`addr & rom_mask`); model2_v.cpp:384-387 / 405-408.

Our display-list tools instead derived the texheader (and UVs) from a `tpa -> id -> table`
lookup (`0x020e0004 + id*16`, entries `{w0=tpa, w1=tha, w2=oba, w3=obc}`), inherited from the
early FIFO-era steps. The lookup returns the **first** table entry whose `w0 == tpa`, whose
`w1` (tha) can differ from this command's actual tha. A VF2_THA_DUMP of colour_f: **38/113
objects differ, 7 with a different colorbase**, including Lau's vest **0x69a890** -- lookup
colorbase 0x01e (colorxlat sub-29 = navy) vs the real `tha` colorbase **0x020** (sub-31 =
red). The `uv_ptr` the lookup produced already equals `tpa` (it reads back the matched entry's
w0), so UVs/geometry are unchanged; only the texture header was wrong.

## Fix

Read the texheader from `tha` and UV points from `tpa`, directly, removing the per-record
lookup in every display-list tool: **geo-colour** (scene render; `tha = w[ptr+1]`),
**geo-fighter** (OBJ + coloured atlas; new `cand_tha[]` from `objs[].tha`), and **geo-scene**
(both the posed OBJ and the akira-variant; new `thaf[]` from `objs[].tha`). `vf2_geo_object`
already parsed `.tha` (w1). Addresses are masked `& 0x7fffff` into the captured texture ROM,
as the working `uv_ptr` already was; the fighter/decor lists use ROM addresses (no `0x800000`
RAM-sourced texheader appears -- decor colour error is unchanged, confirming nothing else
moved). `export-textured` / `export-pieces` read a capture log that records only tpa and are
left as-is; `extract-models` uses a different table and is untouched.

## Validation [MEASURED]

- geo-colour scene vs colour_f/screen.png, mean colour error: **Lau 27.7 -> 24.6 /255**
  (the vest), Akira 25.9 (unchanged), decor 38.7 (unchanged), ALL 33.7 -> 32.8. Lau's vest
  renders red with a green dragon + floral motifs, gold sash and yellow ankle bands, matching
  snap/ref_lau_back_mame.png.
- **Shoes and ponytail of Lau change and improve**: hair/ponytail grey -> **black**, shoes
  dark-grey -> **black** (both were reading the wrong texheader). Yellow ankle bands unchanged.
- f20 geometry/UV overlay unaffected: geo-project is geometry-only (it passes uv_ptr/th_ptr =
  0), and geometry (vertices from oba, UVs from tpa) is unchanged by this fix, so the f20
  silhouette alignment is identical (edge-align score 260.3 at dx=0 dy=-3, the known ~1-frame
  lag). Akira unchanged (navy gi, visible eyes).
- akira.obj / lau.obj re-exported with the RGBA colour atlas.

gcc and clang build clean under `-DVF2_WARNINGS_AS_ERRORS=ON`; targeted geo/display/geometry
tests pass. No capture/texture/PNG/OBJ committed.

## Next

Remaining fidelity items stay: mip/bilinear filtering, perspective-correct UV, and
texture-RAM-sourced texheaders (bit 0x800000) are not captured by these tools (none occur for
the fighters/decor seen so far). `0x581d50` / `0x1a9b54` stay excluded. A fresh vf2 MAME
capture of Lau from the back would confirm the vest colour end-to-end; the scene render
already matches the vf2o reference in shape and dark zones.
