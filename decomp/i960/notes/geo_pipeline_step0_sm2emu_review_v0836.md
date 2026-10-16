# geo_pipeline_step0_sm2emu_review_v0836 — sm2-emu as geometriser reference

**celsowm Step 0 (2026-10-16), geometriser pipeline.** Clone `dmanlfc/sm2-emu`
**outside Git** (here: sibling `..\sm2-emu-ref`, never committed), read its
geometriser / display-list code, compare to `third_party/mame-model2-ref`, and
report what it adds over MAME. **Not compiled, no code copied — reference
reading only.** The MAME MCP stays the execution oracle.

## What sm2-emu is

A from-scratch Sega Model 2/2A/2B/2C emulator in modern C++ (Vulkan/GL/GLES),
**BSD-3-Clause** (same family as this repo), explicitly *derived from MAME*
(`NOTICE`, per-file `// Ported from MAME's src/mame/sega/model2*.cpp` headers).
It is "playable"; the geometry engine is **high-level emulated, exactly as in
MAME** — the 2A geometry ROM has never been dumped, so both reimplement what it
does rather than execute it (`geometrizer.cpp` header; `model2.cpp:1131` "the
Geometrizer is high-level emulated rather than executed").

## Geometriser / display-list: same semantics as MAME, clearer packaging

`geo_process_command`'s opcode table and every handler (`geometrizer.cpp`
2395-…, `geo_matrix_write` 2111, `geo_object_data` 1811, `geo_window_data` 1930,
`geo_texture_data` 1966, …) are a faithful port of MAME's `model2_v.cpp`. So
**sm2 adds no new geometry opcode semantics** over the MAME reference we already
have. What it adds is packaging and instrumentation:

- A standalone `src/hw/geometrizer.{h,cpp}` module with named structs
  (`PolyVertex`, `Polygon`, `QuadM2`, `texheader[4]`, `TextureParameter`) where
  MAME uses macro aliases — a cleaner mental model for our clean-room structs.
- **`GeometryStats`** (`geometrizer.h`): per-opcode execution histogram plus
  `cull_backface/linktype/master_z/behind` and `clip_kill[4]` counters. This is
  a ready-made **verification instrument** for our Step 1/4 success criteria
  (object count, opcode coverage, why a polygon was dropped).
- The driver wiring that MAME buries inline is explicit and commented, which
  settles three things our breakdown was unsure about (evidence below).

## Three wiring facts it makes explicit [evidence]

1. **`geo_parse` runs once per frame at vblank, from the read-start**
   (`model2.cpp:538` `on_vblank_start` → `set_read_start_address` →
   `m_geometry.run()`). So the display list is **single-pass**, not multi-pass:
   our earlier "only 30 objects reachable" was a walker bug (a mis-sized
   opcode), not a multi-list structure.
2. **Double-buffering is just the read-start.** 30/60 Hz and the ping-pong
   halves are handled by which `m_geo_read_start_address` the game set
   (`model2.cpp:538-551`); no special buffer logic — matches the measured
   0x908000/0x918000 alternation. Our Step 2 is therefore trivial.
3. **A prg-port upload gate we missed:** prg `0x804000` writes are pushed to the
   display list **only when not in microcode-upload mode** (`model2.cpp:1128-1138`,
   gated by `m_geoctl & 0x80000000`). Our reconstruction pushed *all* prg writes
   — a real corruption source to fix in Step 1. `push_geo_data` / geo-function
   folding (`model2.cpp:1090-1118`) otherwise matches our reconstruction exactly.

## The headline finding — why the i960-only capture is incomplete [evidence]

The MB86234 coprocessor's IO space is, per sm2's comment, *"the mathematical
lookup tables, or **a window onto the display list buffer**"* (`copro_tgp.h:22-29`,
`copro_tgp.cpp:215-226`), and the copro **writes that buffer**:
`copro_tgp.cpp:308  m_buffer_ram[index] = value;` (external_write into RAM9
`0x900000`). It consumes `fifo_in` (the host `0x884000` stream) and runs
microcode from its 4096-word program RAM.

Consequence: on Model 2/2A the **geometry coprocessor produces display-list
entries and writes them into RAM9 itself.** Our MAME capture records only the
**i960's** writes (`geo/prg/ram9` + `fifoR`); the copro's own RAM9 writes are
not i960 writes, so they are **absent from the capture**. That is the leading,
evidence-backed explanation for why only ~30 objects reconstruct and why they
are exactly the **static stage** (host-authored) while the **dynamic fighter
transforms — computed by the copro — are missing.** (MAME has the same
copro→bufferram path; sm2's modular comments are what made it legible.)

## Impact on the breakdown — revised

Step 0 changes the plan: reconstructing the display list from i960 writes alone
cannot reach the fighter transforms. The cheap unblock is to capture the data
the copro produces, not to model the copro.

- **Step 0 (this note) — DONE.** sm2-emu reviewed; findings above.
- **Step 1 (REVISED) — complete display-list data + exact walker.** Obtain, via
  the MAME MCP oracle, a **per-frame snapshot of RAM9 `0x900000-0x91ffff` taken
  at vblank** (the exact bytes `geo_parse` reads, after the copro has written
  it), for the moving capture's frames — instead of replaying i960 writes.
  Implement the exact-sized opcode walker (all commands, prg-upload gate,
  jump-follow). **Success:** walk ends cleanly, zero unhandled opcodes, object
  count == on-screen objects, and the Akira cluster `0x130a44` (obj `0x19c`) is
  present. *(If a RAM9 snapshot is not available, the fallback is to run the
  MB86234 microcode — program RAM already dumped — against the captured FIFO to
  reproduce its RAM9 writes; much larger, so snapshot first.)*
- **Step 2 — per-object matrix extraction** (opcode `0x0b`/`0x0c`). Success:
  matrix per object; idle-capture stage matrices byte-static; `0x19c` finite.
- **Step 3 — classification + camera separation** across the 9 moving frames.
  Success vs ground truth: `0x19c`/`0x130a44` → FIGHTER, `0x3c1fac` → SCENERY.
- **Step 4 — export** `fighter1/fighter2/stage.obj` (dual-sheet textured).
  Success: partition covers all objects; Akira group holds `0x19c`; opens in the
  viewer (celsowm visual confirm).

(Double-buffering folds into Step 1 — just use the snapshot's buffer.)

## Verdict — what it adds over MAME

- **Accelerates understanding, not semantics.** It is a MAME port, so it gives
  no new geometry behaviour; but its modular `geometrizer.{h,cpp}`,
  `GeometryStats`, and explicit driver wiring made the copro↔display-list
  relationship, the single-pass vblank model, and the prg-upload gate legible —
  which **reshaped the breakdown and saved us from implementing a doomed
  i960-only reconstruction.** Concrete, evidence-backed value over MAME.
- **Does not replace the oracle.** The MAME MCP remains the execution oracle;
  sm2 is HLE like MAME and is kept as *reference reading only*.
- **Legal:** BSD-3-Clause, MAME-derived — compatible with this repo's
  `third_party` policy. A formal `third_party/sm2-emu-ref/` add (like the MAME
  reference) can be decided later by celsowm; for now it stays the uncommitted
  sibling clone, not compiled, not copied.
