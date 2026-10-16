# fighter_scenery_separation_v0834 — per-piece export + measured criteria

**celsowm slice (2026-10-16).** The previous export (`textured_export_v0832.md`)
textured correctly but picked the wrong objects: its `player1.obj` is **stage
scenery, not Akira**, and its "largest record-contiguous cluster = player 1"
rule was an **address-range guess with no evidence**. This slice replaces the
guess with (a) a per-piece export so the fighters can be identified by eye, and
(b) per-object metrics **measured across the 3 capture frames** so any
fighter/scenery split is made from data. No object is labelled a fighter here.

Output is **local only, never committed**; the decode is the same
geometrizer-exact path covered synthetically in CI (`test_texture_link.c`).

## Command

`vf2i960 export-pieces <rom-dir> <capture.txt> <tex0.bin> <tex1.bin> <out-dir>`

For every on-screen object (geo `0x800010` submission) it writes
`piece_<NNN>_obj<id>.obj` (textured, `usemtl sheet0/sheet1` per face), a shared
`pieces.mtl`, the two de-swizzled sheet PNGs, and `pieces.csv`.

## What the capture does and does not carry [MEASURED]

The capture is the live geometrizer stream of frames 1530–1532 (1P fight,
Akira expected). Replayed against the MAME geo protocol reference
(`third_party/mame-model2-ref/model2.cpp` `geo_w`/`push_geo_data`,
`model2_v.cpp` `geo_process_command`): the geo command is
`(opcode>>23)&0x1f` with `0x01/0x11`=object-data `{tpa,tha,oba,obc}`,
`0x0b/0x1b`=matrix-write (12 floats), `0x0f/0x1f`=end. Objects are submitted in
a **stable order** (116 per frame, same order in f1/f2/f3).

Three candidate discriminators were measured per object across the 3 frames:

| signal | meaning | result |
|---|---|---|
| `geo_changed` | `{tpa,tha,oba,obc}` differs across frames (an animating part re-selects its mesh; scenery never does) | **3 / 116** objects (idx 83, 86, 95 — all 2-triangle parts) |
| `reads_f*` / `out_delta` | per-object TGP (0x884000) read-backs and their max float change across frames | only **~17 / 116** carry any per-object transform data; those are low-id overlay/effect objects, not fighter bodies |
| `region` | `oba` source: ROM / fast-RAM / slow-RAM | **114 ROM + 2 slow-RAM** — characters and stage are *both* ROM meshes |

**Conclusion: none of these cleanly separates the two fighters from the stage on
this capture.** Over 3 consecutive round-start frames the fighters are
near-idle, so their pose matrices barely move; the bulk vertex transform is
**front-loaded** (done once at frame start, not interleaved per object, so it
lands against object 0 — hence its 415 reads / delta 600, which is the
camera/global transform, not a fighter); and the camera itself moves, which
would contaminate any global-motion test. 94 / 116 objects have `out_delta == 0`
and no per-object transform recorded — the fighters are among them, unresolved
by motion. celsowm's hypothesis (fighters = matrices that change frame-to-frame)
is sound in principle but **this capture cannot exercise it**.

## Structural grouping [MEASURED] — the usable lead

A character is a contiguous run of records in the polygon ROM. Clustering the
111 unique pieces by `rec_byte` (gap > 0x8000) gives, by triangle density:

| record range | pieces | tris | note |
|---|---|---|---|
| `0x130a44..0x1434a0` | 19 | **682** | densest geometry — **strongest fighter candidate** |
| `0x3c1fac..0x3ca730` | 29 | 524 | the cluster v0832 wrongly called Akira — celsowm confirmed **stage** |
| `0x56c73c..0x56cf6c` | 5 | 80 | |
| `0x15b850..0x1607cc` | 5 | 74 | |
| `0x699aa0..0x69a890` | 3 | 48 | |
| `0x581d50` | 1 | 38 | |
| `0xfb508..0xfc5f0` | 3 | 28 | |
| `0x7804ac..0x780df4` | 15 | 30 | many tiny parts — likely flat/shadow |
| … | | | full table in `pieces.csv` |

This is a *structural* observation, **not** a fighter attribution — the ranges
are reported so celsowm can open a cluster's pieces and confirm by eye which is
Akira / the opponent / the stage.

## How to identify the fighters

1. Open `snap/pieces/piece_*.obj` in the viewer (`--tex0/--tex1` bind both
   sheets) or any OBJ viewer; `pieces.csv` maps each piece index → record range,
   so start from the dense clusters above.
2. Once a cluster is confirmed visually, that **visual proof** is the
   attribution (not an address guess) and `fighter1/fighter2/stage.obj` can be
   cut from it.

## What would let the automated criterion work (for celsowm)

- A capture with the fighters **mid-action** (a thrown punch / step), or frames
  spaced ~10–20 apart rather than consecutive, so pose matrices move measurably.
- Capturing the **per-object matrix-write (opcode 0x0b) operands** before each
  submission rather than only the object-data words, so each object's transform
  is attributable and comparable across frames.

Either lets `export-pieces` promote `geo_changed`/`out_delta` into a reliable
split; the per-piece files + `pieces.csv` are the framework ready for it.
