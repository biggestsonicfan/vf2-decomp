# geo_pipeline_step3c_connected_components_v0854 — 3D grouping; shin is a decode bug

**Geometriser pipeline, Step 3 (grouping).** kazuyette: akira.obj is recognisable
(headband, face, torso, fist, belt, feet) but the legs are missing -- a gap between
waist and feet; the top of the thighs is thin crushed lamellae, knees/shins missing.
Hypothesis: the single vertical screen line (x=243) sends Akira's pieces that cross
it (his front leg reaching toward Lau) to Lau. He asked to (a) check if leg pieces
were classed as shadows or dropped off-screen, (b) check the flatness/column norms
of the crushed thighs, and to replace the line split with 3D connected components.

## (a)/(b) diagnostics [MEASURED]

- **No leg piece is a mis-classified shadow.** Matrix flatness at f20 is perfectly
  bimodal: 30 pieces at 0.000 (shadows), 55 at 1.000 (fighters) -- nothing between.
- **The present leg/thigh pieces are not crushed by the matrix**: every fighter
  piece has flatness 1.000, column norms (1,1,1), mode&3 = 1 (normals present). So
  the "crushing" is not the matrix, the column scale, or the vertex format.
- The two dropped (no on-screen vertex) pieces are `0x000010` and `0xa91d28`
  (tiny / bogus), **not** legs.

## 3D connected components [IMPLEMENTED]

`geo-fighter` no longer splits on a screen line. It decodes every piece's world
vertices (camera removed), computes the pairwise minimum vertex distance, and runs
connected components: pieces of one fighter touch at the joints, the two fighters
are separated. The joint threshold is swept and the one giving the largest
2nd-largest component is kept (both fighters whole, not fused). The two largest
components are the fighter cores; **each remaining small component is attached to the
nearer core** (a crouched fighter's lower leg sits as far from his own torso as from
the opponent -- a ~0.27 knee gap at f20 -- so it forms its own component), as a unit
(so a limb's pieces stay together). Pieces far from both cores are dropped.

Result -- stable on both frames:
- **f20**: 55 pieces, threshold 0.18 -> **Akira 30, Lau 23** (2 bogus dropped).
- **f29**: 55 pieces, threshold 0.24 -> **Akira 30, Lau 23**.

All four Akira foot/leg records (`0x1345ac`, `0x135b48`, `0x135050`, `0x1365c4`) are
on Akira; the face `0x130a44` is on Lau. (At f20 these happen to coincide with the
old line split, since every Akira piece is left of x=243; the 3D grouping matters
when a limb crosses the line, e.g. the f29 lunge.)

## The missing shin is a GEOMETRY-DECODE bug, not sorting [MEASURED]

With the pieces correctly grouped, akira.obj still has a vertical gap. Vertex-Y
histograms (feet at 0, head at top):
- **Akira**: bins [298, **2, 5**, 29, 42, 105, 141, 234, 352, 244] -- a gap at
  **y 0.13..0.40** (the knee/shin).
- **Lau** (same method): [30, 32, 57, 55, 91, 132, 315, 248, 374, 202] -- no gap,
  legs complete.

So Akira is missing a shin that Lau has. The big pieces `0x135050` / `0x1345ac`
(138 verts, 46 tris each) decode to a **flat slab at y 0.00..0.10** despite mode 1,
flatness 1.000 and unit column norms -- i.e. a pure rotation posing already-flat ROM
geometry. So the ROM decode of those records yields crushed/flat vertices: a
**per-record vertex-decode problem** (`ext_emit_piece` reading those records with the
wrong stride/format), not the matrix and not the sorting. Lau's equivalents decode
full, so it is specific to those Akira records.

## Next

Fix the vertex decode for the crushed Akira leg records (`0x135050` / `0x1345ac`
and the missing shin): compare their ROM layout against a correctly-decoded Lau leg
record and against MAME `geo_parse_np_s`, since the matrices and grouping are now
correct. Re-export afterwards.

gcc and clang build clean and produce byte-identical OBJs; vf2_geo_display + the
geometry differential tests pass; the viewer loads both. No capture/texture/OBJ data
committed.
