# geo_pipeline_step3d_shin_is_polygon_ram_v0855 — the missing shin is in polygon RAM

**Geometriser pipeline, Step 3 (shin).** kazuyette's doubt: a flat slab at y 0-0.10
at the floor is probably just the feet (he sees two correct feet on his captures),
not a crushed thigh. He asked for a per-piece overlay of only Akira's pieces, to
locate 0x135050 / 0x1345ac on screen, and -- if a shin is covered by no piece -- to
list every object command in Akira's f20 block including the ones geo-scene ignores
(unresolved, RAM-sourced), and to fix only what the overlay proves.

## The doubt was right -- 0x135050 / 0x1345ac are the FEET [OVERLAY]

New `geo_overlay.py piececolor <shot> <tris> <out> [xmax]` paints each fighter piece
a distinct colour over the screenshot and prints a legend (obj, rec, screen
centroid + bbox). On f20, Akira only (x<243):

- `0x1345ac` bbox screen [134,311..154,323], `0x135050` [193,316..220,331] --
  both at the **floor** (screen y ~ 311-331), i.e. Akira's **feet**. My Step-3c
  claim that they were crushed thighs was **wrong**: they are correctly-decoded
  feet (flat because feet are flat, on the ground). Their decode is fine.

## The shin is covered by no piece [OVERLAY]

The same overlay shows Akira's pieces cover the thighs down to **screen y ~ 276**
(`0x13b998` [158,228..186,276], `0x13c02c` ..264, `0x62f7d8` ..260) and then the
feet at **y ~ 308-331** -- with a clear gap at **y 277-307**, where the screenshot's
blue trouser (shin/knee) shows through uncovered. So Akira's shin is drawn by no
decoded piece. Lau, by contrast, has continuous leg coverage (`0xfc5f0`
[325,271..366,312], `0x130a44` ..298) -- **Lau's shin is a ROM piece and is
present** (his upright stance also makes the shin short).

## Why -- Akira's shin is in polygon RAM, not ROM [MEASURED]

Listing every f20 object command with its polygon source (MAME geo_object_data: oba
bit24 = fast polygon RAM, bit23 = polygon ROM, else slow polygon RAM): of 113
objects, **111 are ROM and exactly 2 are polygon RAM0** -- `0x000010`
(oba 0x00000004) in Akira's block and `0x000410` (oba 0x00000104) in Lau's. Our
RAM9 capture holds the display list and the polygon **ROM**, but **not** the polygon
RAM, so a RAM-sourced piece cannot be decoded here. Akira's shin is that RAM0 piece;
there is no ROM record for it to mis-decode -- so this is **not** a vertex-decode
bug, and nothing in the ROM path needs changing.

## Fix (only what the overlay/list proves)

`geo-fighter` now tests the oba source bits and **skips RAM-sourced pieces**
(bit24 set, or bit23 clear) instead of reading ROM garbage at their record, and
reports the count. f20: 53 ROM fighter pieces; Akira 30, Lau 22; "2 RAM-sourced
piece(s) skipped ... e.g. a shin; needs a polygon-RAM capture". akira.obj is
unchanged (the RAM0 piece was already not selected); the feet stay. No change to the
vertex decode -- the overlay proved the feet decode correctly and the shin has no
ROM geometry to fix.

## Next

To get the shins, capture the polygon RAM (MAME `geo->polygon_ram0` / `polygon_ram1`,
the slow/fast polygon buffers) alongside RAM9, and point the decoder at RAM for oba
bit24/!bit23 pieces. Then re-export; the fighters should be leg-complete.

gcc and clang build clean and produce byte-identical OBJs; vf2_geo_display + the
geometry differential tests pass; geo_overlay.py parses. No capture/texture/OBJ/
overlay data committed.
