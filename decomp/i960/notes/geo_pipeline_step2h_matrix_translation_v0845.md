# geo_pipeline_step2h_matrix_translation_v0845 — fighter matrix translations

**Geometriser pipeline, Step 2h.** Step 2g showed the fighter geometry decodes to
the right size (unit column norms, tiny vertices at Z~4.7) and is mis-*placed*
horizontally, so the error is in the fighter matrix translations. kazuyette asked
to chase those and re-examine how the host builds the 0x0b matrices. Frame 20 of
`vf2_fight_ram9_screen_30f` (local).

## The walker is MAME-exact; the matrices are the host's own [VERIFIED]

- `geo_matrix_write` (0x0b, model2_v.cpp:2007) writes the 12 floats **absolutely**
  into `geo->matrix` — no stack, no concatenation. `geo_translate_write` (0x0c)
  overwrites only [9..11]. So there is no relative/push-pop hierarchy the walker
  is flattening wrongly: the host (i960/TGP) bakes each object's full world matrix
  and writes it absolute, and the walker binds the last-written matrix to each
  object (Step 2c: 112 x 0x0b, no 0x0c/0x0d/copro). MAME-exact.
- `geo_object_data` applies no scale; the window centre is uniform (248,319) with
  center_sel 0 for all 113 objects. So the only placement input is the matrix
  translation (Tx,Ty,Tz) = matrix[9..11].
- f20 readstart is 0x00000000, a legitimate bank-0 start (the readstarts cycle
  0/0x8000/0x10000/0x18000 across the 4 RAM9 banks), and yields 113 coherent
  objects, so the matrix<->object pairing is sound.

`geo-project` now emits the per-object translation as CSV columns `tx,ty,tz`.

## Decor is one shared matrix; fighters are per-limb [MEASURED]

- **All 25 decor objects share one matrix**: Tx=2.794, Tz=4.565 (identical). The
  stage layout lives in the decor *vertices* (rawext 6..97), posed by that single
  matrix. This is why decor registers: it is effectively one big model.
- **Fighters have a distinct matrix per object** (the skeleton is in the
  translations; vertices are tiny, rawext 0.1..0.5). Fighter body Tx spans
  -0.83..+0.93, **symmetric about ~0**, so with `screen_x = 84 + 248 + Tx*600/Tz`
  (Tz~4.65) they project about **screen-centre 332**.

## The fighters are X-shifted, not compressed [MEASURED]

Using `screen_x ~= 332 + 129*Tx`:

- fighter **body** Tx median -0.302 -> screen **293**
- fighter **shadow** Tx median -0.739 -> screen **237**
- on-screen feet (screenshot shadow centroids) midpoint ~**247**

So the fighters' **shadows (feet) land about right (~237 vs 247)**, but the
**upright body matrices sit ~56px to the right of their own shadows** (293 vs 237).
The body sub-groups' internal separation (~110px) matches the on-screen Akira<->Lau
gap, so it is a **rightward group offset of the bodies (~0.44 world-units in Tx),
not a compression**. This finally localises the long-running ~85px: it is an
X-axis offset carried by the fighters' upright body matrices specifically (the
decor and the fighters' own shadow matrices do not have it). It supersedes the
Step 2f "1.5x compression" reading.

## The two Z~0 records are mis-grouped decor [MEASURED]

`0x581d50` and `0x1a9b54` carry the **decor matrix exactly** (Tx=2.794, Tz=4.565)
and decode decor-scale geometry (rawext 48/12). Their predicted centre is
screen_x~699 (off-screen right) and their posed vertices straddle Z=0 (zmin
-19/-3), exploding the bbox. They are decor-matrix objects that the projected-side
classification lumped into "fighter"; they should be culled before the fighter
split, not treated as fighter parts.

## Next

Find the source of the ~0.44-unit (~56px) Tx offset between the fighters' upright
body matrices and their own shadow matrices (and the decor): the bodies render
right of the feet, so the host's fighter-body matrix carries a translation the
feet/shadow matrix does not. Compare, per fighter, a body limb's matrix[9..11]
against the matching shadow's, and check the i960 code path that builds the fighter
body vs shadow matrices. Separately, cull `0x581d50`/`0x1a9b54` (decor matrix) from
the fighter set. Then camera-free fighter/scenery classification + export.

gcc and clang produce byte-identical projection PNG + CSV + tris; vf2_geo_display
and the geometry differential tests pass. No capture/screenshot/projection data
committed.
