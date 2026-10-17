# geo_pipeline_step4b_colour_textures_texparams_v0860 — colour OBJ textures, persisted texparams, specular

**Geometriser pipeline, Step 4 (colours), follow-ups.** kazuyette, after the colour
render (v0859): (1) export the fighters with **colour** textures referenced by the
MTL; (2) build a tool to find command **0x06** across a series of lists and apply
the most recent before colouring; then kazuyette captured the real 0x06 (persisted
since boot) and asked to apply it as the engine's initial state and re-compare.

## 1. Coloured fighter textures [DELIVERED]

`geo-fighter` gained an optional final arg, a colour-capture dir. With it, besides
the OBJ it bakes a per-bank **albedo atlas**: for each polygon, colour every texel of
its texture rectangle through the shared `gc_texel_rgb` (texel → luma tone curve →
shade → `palram[colorbase+0x1000]` 5-5-5 → colorxlat per channel → VF2 gamma) at
**full lighting** (lumascale 0xff, no per-polygon lighting so a shared texel does not
conflict). The colour pipeline (gc_u16 / gc_gamma / gc_texel_rgb) is now shared
between geo-colour and the export; the geo-colour PNG is **byte-identical** after the
refactor. Sheets are per-fighter (`<which>_tex0/1.png`) since each paints only its own
pieces. The atlas shows correct materials: Akira's skin face / orange hair / navy
cloth / brown eyes, Lau's green outfit, red belts.

- Local outputs (never committed): `..\snap\fighters_obj_colour\{akira,lau}.obj` +
  `{akira,lau}_mtl` + `<which>_tex1.png` (both fighters use sheet 1 only; tex0 empty).
- Viewer note: the built-in `vf2_model_viewer` only reads raw banks (grayscale, R8).
  For colour, open the OBJ in any standard OBJ viewer — it follows the MTL `map_Kd`
  to the coloured PNG. Adding an RGB-PNG path to the in-engine viewer (shader +
  format change) is a possible follow-up.

## 2. `geo-texparams` — find and persist command 0x06 [REFERENCE-VERIFIED]

`vf2i960 geo-texparams <ram9> <readstarts> <out.txt>` walks every frame in order,
applies each 0x06 (two words per entry: `[param][coef]`), and writes the **persisted**
state — the most recent `diffuse ambient spec_scale spec_ctrl` for all 32 indices —
plus the last `geo_mode` (command 0x07). kazuyette's capture (`snap/tparams`) shows
the 0x06 is sent **once at boot** (frame 1846, zone 0, offset 0x10, index 0, count 32)
and never again through round-1 start (frame 4229): the table persists for the whole
fight (MAME value-inits `geo_state` to 0/0 and only overwrites it).

Running the tool on `ram9_f01846.bin` reproduced kazuyette's decode **exactly** (idx 0
127/63, idx 2 48/128, idx 10 87/158, idx 25 0/205, idx 28 255/255, idx 31 0/255) and
reported `geo_mode = 0x1` → parser `mode&3 == 1` = **specular**.

## 3. Apply persisted texparams + specular to the colour render [MEASURED]

`geo-colour` gained an optional final arg (the texparams file) and now tracks
`geo_mode` (0x07) and applies the specular term exactly as `geo_parse_np_s`
(model2_v.cpp:1243-1252): `spec = max(2·dotl·nz − light.z, 0)`, squared per
`specular_control` bit, × `specular_scale`, only when `(mode&3)∈{1,3}`.

Mean per-pixel colour error vs `screen.png` (drawn pixels only), frame 3002:

```
                defaults 255/0      with persisted texparams
akira               31.3                26.0
lau                 37.6                27.6
decor               51.7                38.5   <- drops notably
ALL                 44.6                33.5
```

The decor drop is from the real **ambient** term (≈63, vs our 0 default); the fighters
improve too. Specular is active (geo_mode 1) but touches only the few shiny materials
(idx 8/25; idx 28 is ambient-saturated at 255) — 6 polygon-terms this pose, so no
measurable pixel change here, but it is faithful and will show where those materials
are prominent. `coef_table` is the LOD distance coefficient; it affects only
mipmapping, which this renderer does not do, so it is recorded but unused.

## Build / tests

gcc and clang build clean; the geo-colour PNG (default and with-texparams), the
`texparams.txt`, and both fighters' OBJ + coloured atlas are **byte-identical** across
compilers. geo-colour's default path is byte-identical to v0859. Geometry
differential + display tests pass (no exporter decode changed). No capture / texture /
PNG / OBJ data committed. The kit system fiche `.._kit/systems/model2.md` is updated
(RAM9 4 lists, projection, decoder, 0x05 polygon RAM, colour path, texparams
persistence + specular).

## Next

If the in-engine coloured viewer is wanted, add an RGB-PNG texture path to
`vf2_model_viewer`. Otherwise the colour pipeline is complete; remaining fidelity
items are mip/bilinear filtering and perspective-correct UV. `0x581d50` / `0x1a9b54`
stay excluded.
