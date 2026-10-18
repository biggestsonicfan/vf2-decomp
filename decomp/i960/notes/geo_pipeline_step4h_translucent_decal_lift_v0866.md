# geo_pipeline_step4h_translucent_decal_lift_v0866 — lift translucent decals off the base (Lau's dragon)

**Geometriser pipeline, Step 4 (colours), OBJ defect.** kazuyette, in Blender: Lau's back
now shows the green dragon on the red vest (Step 4g), but three defects remain: (1) a black
rectangle left of the dragon, (2) a white/blue circle (the pearl) cut into quarters that
don't connect, (3) the dragon looks incomplete. Hypothesis: texheader mirror bits.

## Diagnosis [REFERENCE-CHECKED + MEASURED]

**Not mirror.** Every polygon of the vest record 0x69a890 has `mirror=0,0 wrap=0,0`. The
grayscale tile dump shows the dragon tile (sheet0, 256,768 256x256) is a *complete* dragon
head with a blue/white pearl (lower-right) and a Chinese seal stamp (lower-left) -- the
"rectangle" is that seal, the "pearl" is the ball. The baked colour atlas reproduces it
perfectly (green dragon, blue pearl, orange seal). Rendering only the dragon-tile polygons
(by UV region) shows the dragon complete and correct, pearl round, seal orange. So the atlas
and the UVs are right.

The real cause: the vest record interleaves **opaque base** polygons (renderer bit1 only:
the red fabric + roundel tiles 0,640 / 192,640 / 0,896 / 128,640) with **translucent decal**
polygons (renderer bit0 set: the dragon tile 256,768 and the ornament 384,0), and the decals
are **coplanar** with the base. MAME draws translucent polygons over opaque ones (list order
+ z-buffer), so the decal wins. A baked OBJ in a plain z-buffered viewer z-fights the two
coplanar layers, so the red base overdraws most of the dragon -- leaving fragments, and
splitting the pearl/seal where the fight goes one way or the other (the "quarters").

## Fix

In `ext_emit_piece`, keep the per-primitive unit normal (previously skipped) and, for a
translucent polygon (`renderer & 1`), lift its triangle by a small distance along that normal
(rotated into world space) via a new `onrm`/`eps` pair on `ext_emit_tri`. `eps = -0.003`
world units (~0.2% of a fighter's ~1.5-unit height; the sign is -normal because VF2's stored
normals point into the body here) -- enough to win the depth test from any angle, too small to
see as displacement. Opaque polygons are unchanged (`eps = 0`). This reproduces MAME's
"translucent over opaque" without per-pixel draw-order, and it pairs with the Step 4f alpha
cutout: around the dragon the decal texels are 0x0f (alpha 0), so the red base shows through
while the dragon itself sits on top.

Only the OBJ export changed; the scene render, atlas baking and texel/colorbase/luma decode
are untouched.

## Validation [MEASURED]

- Lau's back, baked OBJ (alpha-clip render): the dragon is complete and on top of the red
  vest, the pearl is a single round blue/white ball, the seal is the orange stamp -- all three
  defects gone, matching snap/ref_lau_back_mame.png. Lau's front shows the gold/green frogging
  decals correctly over the red vest. Akira unchanged front and back (navy gi, visible eyes,
  grey headband tails, black hair/shoes) -- no decal hidden or displaced, so the normal-sign
  choice is consistent across both fighters.
- Scene colour error vs colour_f/screen.png is byte-unchanged (lift is OBJ-only): Akira 25.9,
  Lau 24.6, decor 38.7, ALL 32.8.
- akira.obj / lau.obj re-exported.

gcc and clang build clean under `-DVF2_WARNINGS_AS_ERRORS=ON`; targeted geo/display/geometry
tests pass. No capture/texture/PNG/OBJ committed.

## Next

A genuine mirror-bit texture (bit 8/9 set) would still need the per-pixel reflection handled
(bake the mirrored half, or fold UVs) -- none appears on Lau/Akira so far. Remaining fidelity
items: mip/bilinear filtering, perspective-correct UV, texture-RAM-sourced texheaders
(0x800000). `0x581d50` / `0x1a9b54` stay excluded.
