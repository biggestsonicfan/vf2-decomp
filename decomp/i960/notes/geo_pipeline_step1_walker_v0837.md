# geo_pipeline_step1_walker_v0837 — display-list walker, validated on RAM9

**Geometriser pipeline, Step 1 (plan in `geo_pipeline_step0_sm2emu_review_v0836.md`).**
The display-list opcode walker (`vf2_geo_walk`, `include/vf2/geo_display.h`,
`src/platform/geo_display.c`) is implemented, unit-tested synthetically in CI,
and now **validated against a real RAM9 capture**.

## Capture (local, never committed)

Local RAM9 capture taken with the MAME MCP — `snap\vf2_fight_ram9_p1move_30f.bin`
+ `.txt`: 30 consecutive frames of
a round-1 fight, P1 holding Right, Punch pulse ~idx 10, Kick ~idx 20. The `.bin`
appends, per frame, the full RAM9 `0x900000-0x91ffff` (131072 bytes, u32 LE)
snapshotted in MAME's frame-done notifier right after `geo_parse`. The `.txt`
gives per frame `F idx frame readstart` (the `0x803008` read) plus the i960
writes to `0x801008`/`0x803008`/`0x980000`.

**Measured RAM9 layout:** FOUR 32 KiB display lists, read-start cycling
`0x0 / 0x8000 / 0x10000 / 0x18000` (sometimes repeated frame to frame). Each list
begins `04000000 40800000 03800000 00000003` = zsort-mode(`0x40800000`) +
geo-mode(`0x00000003`), two single-operand commands the walker sizes exactly. No
`geoctl` writes during the fight (so the prg gate never closes here).

## Validation tool

`vf2i960 geo-walk <ram9.bin> <frame-index> <readstart-hex>` loads one frame's
list, walks it with `vf2_geo_walk`, and reports stop reason, object count, the
list header, and whether Akira's record cluster `0x130a44..0x1434a0` (which holds
obj `0x19c`, his face — recognised by eye by kazuyette) is among the objects. Local-only; the
walker is covered synthetically in CI (`test_geo_display.c`).

## Result — Step 1 criterion met [MEASURED]

Walking each of the 30 frames from its own read-start:

- **Clean traversal:** every frame stops on `END` (0x0f/0x1f), ~274 opcodes, no
  `BOUNDS`/`OPCODE_LIMIT`, header exactly `04000000 40800000 03800000 00000003`.
- **Object count = objects on screen:** **113** object submissions per frame,
  stable across all 30 frames. (The earlier FIFO-capture counted 116 geo-0x800010
  writes; the authoritative on-screen count from the final parsed list is 113 —
  the few extra submissions are not in the parsed list, consistent with
  double-buffered list building.)
- **Akira present:** his cluster `0x130a44..0x1434a0` holds **18** objects in
  frames 0-24 and **19** in frames 25-29 (a part added as the kick extends) —
  present in every frame, as required.

Single-pass per frame from the read-start, exactly as `geo_parse` runs at vblank
(`geo_pipeline_step0...v0836.md`); the four-list cycle is just which read-start
the game set, no special logic. Step 1 is complete and validated under gcc and
clang.

## Next — Step 2

Extract each object's transform matrix (the walker already tracks it) across the
30 frames; success: Akira-cluster matrices vary (he moves) while stage matrices
stay static, with the camera separated out (Step 3). Backup still-capture
`vf2_fight_ram9_moving_30f.*` is available as a no-P1-action control.
