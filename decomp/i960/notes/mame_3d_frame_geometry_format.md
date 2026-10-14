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

## Format finding: the opcode is 7 bits (word>>23)&0x7f, not 5

Examining the FIFO headers: a command header is self-describing -- its low byte
equals `(word>>23)&0xff` and byte2==byte3 (e.g. `0x00800101`, `0x03000606`,
`0x05800b0b`, `0x13802727`, `0x1f803f3f`, `0x35806b6b`), while payload floats do
not. The opcode is therefore `(word>>23)&0x7f` (7 bits), e.g. `0x13802727`->0x27,
`0x05800b0b`->0x0b, `0x1f803f3f`->0x3f. The plan's Phase 6 text uses
`class = (word>>23)&0x1f` (5 bits), which **conflates** distinct opcodes
(0x07/0x27/0x47/0x67 all map to 0x07, etc.) -- this is exactly why `tgp.c`'s
5-bit parser mis-frames the real stream. (geometry-census's 5-bit tally is still
fine for the attract wait, which carries no real opcodes, but 3D needs 7 bits.)

A naive low-byte==opcode discriminator over the whole frame still over-matches
(zero words and coincidental payload also satisfy it), so a clean frame needs
the per-opcode payload lengths, which come from the opcode semantics. That is
the part that needs either the MB86234 behaviour or a reference output to pin
(below) -- the 7-bit opcode and the self-describing header are the verifiable
in-tree findings so far.

## Verified opcode payload lengths (self-consistent framing)

Counting payload words between consecutive command headers (header =
`op<<23|op<<8|op`) across the frame pins the fixed-length opcodes:

| opcode | payload words | samples | role |
| --- | --- | --- | --- |
| 0x0b | 12 | 2/2 | 3x4 transform matrix (matches the reference model) |
| 0x34 | 1 | 89/89 | (single data word) |
| 0x01 | 0 | 117/117 | object -- model referenced via geo/prg, not inline |
| 0x02 | 0 | 119/119 | direct |
| 0x13 | 0 | 54/54 | (header-only) |
| 0x00 | 0 | pad | zero words, header-only |

Encoded (verified) in `vf2_tgp_command_payload_words()` with a unit test; every
other opcode returns -1 (fail closed). Note the end marker is 7-bit opcode
**0x3f** (`0x1f803f3f`), NOT 0x1f -- the older 5-bit decode read it as 0x1f; its
length is not independently pinned here, so it is left -1. Variable-length
opcodes measure 0..3 payload words data-dependently (e.g. 0x29 n=54, 0x37 n=69,
0x39 n=60) and are NOT pinned from one frame -- that framing, and the triangle
assembly, need the reference output below.

The whole stream frames by header delimitation (payload = words to the next
header), a rule consistent with every pinned fixed length; `vf2_tgp_next_command`
implements it (fixed length where pinned, delimitation otherwise, with
`length_verified` flagging which), with a unit test over a synthetic stream.

## What is needed to validate

A MAME **reference triangle list** (or the rendered image) for the same frame(s)
-- i.e. the transformed/projected vertices and the per-triangle texture/colour
ids MAME produces from this stream -- so the native pipeline's output can be
diffed against it. The captured input stream alone pins the format but not the
expected output; the per-pixel/per-vertex reference is the oracle the Phase 6
exit compares against.
