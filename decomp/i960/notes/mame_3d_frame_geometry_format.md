# Real 3D geometry frame (MAME capture): structure and the viable Phase 6 path

Date: 2026-10-14. Analysis of a MAME vf2 reference capture of one attract 3D
frame (user-supplied, `snap/vf2_attract_frame_geometry_f1000.txt`, local
reference only -- never committed). This reframes Phase 6: a real 3D geometry
stream is now in hand, so the pipeline can be built against it directly, without
first getting the native attract past the TGP self-test (see
`attract_stuck_in_tgp_test_phase.md`).

## The capture

One frame of i960 writes, 4 streams:

- **`prg` (0x00804000 family), 1128 words -- the vertex buffer.** Writes cycle
  through `+0/+4/+8` as `x,y,z` float triples (model-space; magnitudes ~0.03),
  with periodic `+c` attribute words (e.g. `0x00901502`, `0x00040401`), after a
  small header (`0x00000100`, `0x000000c9`, `0x00040401`). Per-subaddress counts
  365/281/281/201 => ~281 vertices, ~201 with a 4th component.
- **`geo` (0x00800000 family), 378 words -- object-table pointers / ring.**
  Writes to `0x00801008` (object addresses `0x86ec`, `0x8720`, ...) and
  `0x00800010` (`0x00488b06`, `0x005909de`), plus geo ring bookkeeping.
- **`fifo` (0x00884000), 2684 words -- the command stream.** Every class
  `(word>>23)&0x1f` 0x00..0x1f is present: class 0x01 (object) x176, 0x02
  (direct) x183, 0x0b (matrix) x25, 0x1f (end) x136, and many others. Matrix
  headers look like `0x05800b0b` / `0x15802b2b` / `0x35806b6b`.

Contrast with the attract wait (geometry-census): there the stream is only ring
bookkeeping + copro-FPU camera scalars, no mesh. Here (3D) it is a full mesh:
vertices + matrices + objects + end markers.

## Why Phase 6 Option 1 is viable with this

The ingredients to produce triangles are all present in the stream: model-space
vertices (`prg`), transforms (FIFO class 0x0b matrices), and object/draw commands
(FIFO 0x01/0x02, `geo` pointers). `src/hardware/tgp.c` already has the math --
`vf2_tgp_project_vertex` (4x4 * vertex, perspective divide, viewport) and
`vf2_tgp_render_triangle`. So the native pipeline does not need the TGP microcode
to *transform* (the host applies the stream's matrices to the stream's vertices);
it needs the real **command format**, which the current `tgp.c` parser does not
have: feeding the captured FIFO stream to `vf2_tgp_scan_geometry_stream` fails
immediately (rc=UNSUPPORTED) on class 0x12, a class the hypothesized parser never
modelled.

## The work (Phase 6, now unblocked on input)

1. Recover the real FIFO command format from this capture (and ideally 2-3 more
   frames): per class, its length/framing and role (matrix load, object draw,
   vertex-buffer bind, attribute/material, end), and how `prg` vertices and
   `geo` object pointers associate with the FIFO draw commands.
2. Build the native pipeline: parse the stream, apply the class-0x0b matrices to
   the `prg` vertices, project and assemble triangle lists (reuse the `tgp.c`
   primitives; extend as needed for texture/colour ids from the attribute
   words).
3. Validate per the plan's Phase 6 exit: triangle lists (count, vertices to 1/16
   pixel, texture+colour ids) equal to the reference for the captured frames.

## What is needed to validate

A MAME **reference triangle list** (or the rendered image) for the same frame(s)
-- i.e. the transformed/projected vertices and the per-triangle texture/colour
ids MAME produces from this stream -- so the native pipeline's output can be
diffed against it. The captured input stream alone pins the format but not the
expected output; the per-pixel/per-vertex reference is the oracle the Phase 6
exit compares against.
