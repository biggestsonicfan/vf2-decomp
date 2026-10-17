# geo_pipeline_step3b_fighter_split_by_position_v0853 — sort fighters by screen

**Geometriser pipeline, Step 3 redo.** kazuyette's visual check of the Step-3 OBJs:
lau.obj recognisable from behind (ponytail, arms, legs) but **missing torso and top
of head**; akira.obj only ~10 incoherent pieces. Same method gives a coherent Lau,
so the **family sort** was suspect. Expected ~25-30 pieces each.

## Why the family sort was wrong [MEASURED]

Projecting every fighter piece at f20 (xoff=0) and reading its screen x shows the
record families are **shared between both fighters**, so a family sort cannot work:

- **family 0x13** holds Akira's parts (12 pieces, left) AND **Lau's face
  `0x130a44`** and head `0x1314e8` (right) -- the old sort sent Lau's head to Akira.
- **family 0x00** (tiny shared primitives) splits 6 Akira / 2 Lau.
- families the old sort **excluded** actually belong to fighters: `0x08`/`0x0b` ->
  Akira, `0x2c`/`0x29` -> Lau.

So the old sets gave Akira 24 and Lau 18 (42 of 53), dropping 11 real pieces incl.
Lau's head, and mixing Akira. Record range is not identity -- the standing rule
("separate by position, not record range") applies here too.

## Fix -- split by on-screen position [MEASURED]

`geo-fighter` now projects every non-shadow, resolvable fighter piece (the two
decor-matrix records `0x581d50`/`0x1a9b54` out) to its on-screen centroid x (centre
248, focus 600, xoff 0), drops pieces with no vertex on screen (degenerate / off
-frame -- e.g. the bogus `0xa91d28` that had pulled the split to the screen edge),
splits the two fighters at the **widest gap** in the sorted x (each side >= 5
pieces), and takes `akira` = left group, `lau` = right.

Frame 20: 55 candidates, **split at x=243 (gap 30)** -> **Akira = 30 pieces** (x
143..228), **Lau = 23 pieces** (x 257..368); 2 off-screen pieces dropped. The face
`0x130a44` now lands at x=257 -> **Lau** (verified: it is in lau.obj, not akira.obj).
Lau regains its head (`0x130a44` face, `0x1314e8`) and the `0x2c`/`0x29`/`0x00`
pieces; Akira regains `0x08`/`0x0b`/`0x00` pieces.

The split holds on a far frame: **f29 -> split x=298 (gap 35), same 30 / 23** (the
split tracks the fighters as Akira lunges right). Piece counts are stable; the
camera-removal, feet-centring and texturing are unchanged from Step 3.

## Result

- `..\snap\fighters_obj\akira.obj`: 30 pieces, 484 tris, 1452 verts, bbox ~0.77 x
  1.33 x 0.69 (feet-centred, upright, camera-free).
- `..\snap\fighters_obj\lau.obj`: 23 pieces, 512 tris, 1536 verts, bbox ~0.91 x
  1.52 x 0.69.

Viewer (from `vf2-decomp`):

    build/vf2_model_viewer.exe ../snap/fighters_obj/akira.obj \
        --tex0 ../snap/vf2_fight_tex0_f1808.bin \
        --tex1 ../snap/vf2_fight_tex1_f1808.bin

(same with `lau.obj`). `--selftest`: akira 1452 verts, lau 1536 verts, textures
bound OK.

## Next

Eyeball both meshes again (joint seams still expected -- per-object world matrices,
no skinned skeleton). The two dropped off-screen pieces per frame can be revisited
if any is a real part clipped at frame 20. Then the stage/decor export.

gcc and clang build clean and produce byte-identical OBJs; vf2_geo_display + the
geometry differential tests pass. No capture/texture/OBJ data committed.
