# geo_pipeline_step5b_stage_textures_follow_opponent_v0877 — geo-stage for any arena; stage textures/palette follow the opponent, not 0x500064

**Geometriser pipeline, Step 5 (decor).** kazuyette forced the stage byte `0x500064`
to 1..10 (arcade continue, opponent always Lau) and captured `snap\stage_01..10`.
Export them, and explain why stages 4/6/7/9 have wrong textures/palettes (green
checker floor, rock everywhere) while the geometry is right.

## geo-stage generalised to any arena

The decor cull used the stone-towers record range `0x3c1fac..0x3ca730`, which is
arena-specific, so geo-stage found no decor matrix on the new stages. Fixed:
**stage mode now detects the decor matrix as the one the most non-shadow objects
share** (the decor all carry the camera matrix; each fighter limb has its own), and
keeps the ROM objects carrying it (deduped). Arena-independent. The fighter path
(stage_mode 0) keeps the record range, byte-identical. On the stone-towers stage this
also recovers one object the record range missed (`0xa7c9f8`, shares the camera
matrix), 20 -> 21.

Exported 10 arenas (local, snap/stages_obj_colour/stage_NN): 15..27 decor objects
each, geometry on-model vs each screen.png.

## Why 4/6/7/9 look wrong: textures/palette follow the OPPONENT, not 0x500064 [MEASURED]

`0x500064` drives only the **decor models + camera** (step 5a / `stage_disp`). The
stage's **textures and palette are selected per fight from the opponent's character**:

- Capture evidence: with the opponent fixed at Lau across all 10 forced stages,
  `tex0/tex1` collapse to **two** banks (01-02 vs 03-10), not ten -- the texture bank
  was **not** reloaded per forced stage. The 4 "wrong" arenas (04/06/07/09) carry the
  **identical** bank as 4 "correct" ones (03/05/08/10); each arena's palette differs
  but is mismatched for 4/6/7/9. So the decor moved while its textures/palette stayed
  Lau's.
- Code path: the fight-entry routine at `0x00042510` computes a texture/palette **set**
  as `ROM_table_0x0003cdb4[(char & 0xf)*K + (stage & 0xf)]` (a second table at
  `0x0003ce24` for another sub-case) and stores it to the fighter task record at
  `g13+0x9e`. `g13` is a per-entity **task record** (measured in the audio notes at
  `0x514940 / 0x515d80 / 0x516480`), and **`g13+0xAF` is that entity's character
  index** (read all over the fight code, e.g. `enemy_control`'s per-character table
  `0x0003d638`). So the set is a function of **(character, stage)**; freezing the
  opponent at Lau while sweeping the stage gives the wrong set wherever the correct
  arena belongs to another character.

## What to force in addition [for the recapture]

The opponent's character must match the arena. In VF2 the arena and its owner
character are paired, so:
- **Simplest:** pick the matching opponent at the P2 select screen (the cursor
  `0x515c86`/`0x515cc0` kazuyette already drives) instead of always Lau, then
  `0x500064` is redundant -- selecting opponent N loads arena N's decor **and** its
  textures/palette together. Or let arcade progress naturally to that opponent.
- If keeping the forced-decor method, also set the P2 fighter's character index
  (`g13+0xAF` of the P2 task record) to the arena's owner, so the
  `table_0x3cdb4[char*K+stage]` lookup returns the matching set. That index is seeded
  from the P2 selection, so forcing the P2 character at select-confirm (as `0x500064`
  is forced) is the practical lever.

## Stage -> owner character (arcade pairing table, ROM 0x12508) [MEASURED]

The arcade match setup reads the round index at `0x00500054`, then
`ldob 0x00012508[round*2]` -> stage (stored to `0x500064`, at `0xcf6c`) and
`ldob 0x00012509[round*2]` -> opponent character (at `0xce68`). So ROM table
`0x00012508` is a stride-2 list of `(stage, opponent-char)` per round. Inverting it
(char index = select order Akira0/Pai1/Lau2/Wolf3/Jeffry4/Kage5/Sarah6/Jacky7/Shun8/
Lion9/Dural10):

| stage (0x500064) | owner character |
|------------------|-----------------|
| 0 | Jeffry (/Lau) -- stone towers, the VS default |
| 1 | Shun |
| 2 | Sarah |
| 3 | Dural |
| 4 | **Kage** |
| 5 | Wolf |
| 6 | **Pai** |
| 7 | **Jacky** |
| 8 | Akira |
| 9 | **Lion** |
| 10 | Akira (the "variant of 10" round) |

So to capture a stage with correct textures/palette: set P2 to the paired character
(+ force `0x500064`), since the texture/palette set `0x42510` picks follows the P2
character. For the four wrong arenas: 4=Kage, 6=Pai, 7=Jacky, 9=Lion. (The ROM region
is reconstructed from the four maincpu EPROMs with the VF2_LOAD32_WORD interleave
from `rom_manifest.c`; char-index numbering corroborated by char 8 = Shun being the
one odd entry in the `0x3cdb4` texture table.)

No ROM/capture/OBJ committed; stages local in snap/stages_obj_colour. gcc+clang clean
under `-DVF2_WARNINGS_AS_ERRORS=ON`; geo/display/geometry tests pass.
