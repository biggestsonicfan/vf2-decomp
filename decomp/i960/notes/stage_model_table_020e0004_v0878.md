# stage_model_table_020e0004_v0878 — the model-ID -> object-record table (for a from-ROM stage export)

Goal: export a stage's full decor from ROM (not just the ~75% a single frame's display
list carries). Needs the table that turns a model ID into the display-list object
command (tpa, tha, oba, obc). Found and validated from the ROM alone.

## Model table [MEASURED — validated]

The object submitter `0x00007c60` (called by `stage_disp` with a model ID in g0) does:
```
00007cb8  lda 0x020e0004[g0*16], g0   ; entry = 0x020e0004 + id*16
00007cc0  ldq (g0), r8                ; r8..r11 = the 4 object words
...       stq r8, (g10)[g12]          ; written to the display list
```
So the **model table is at `0x020e0004`** -- i.e. the MAIN_DATA region (base
`0x02000000`, ROMs mpr-17560/17561) at **offset `0x000e0004`** -- with **16-byte
entries `{tpa, tha, oba, obc}`** indexed by model ID. `oba` carries the polygon bit
(`0x800000`); the record byte address is `(oba & 0x7fffff) * 4`.

**Validation:** `id 0x494` -> entry `tpa=0x001b444c tha=0x001051b4 oba=0x008f07eb
obc=0x0006000a` -> record `(0x8f07eb & 0x7fffff)*4 = 0x3c1fac`, exactly the captured
stone-towers record (oba `0x8f07eb`). The reverse map also resolves a whole capture:
the arcade stage-4 (Kage) records `0x599e64..0x5a03a8` map to model IDs `0x87d..0x8a6`.

## Per-stage descriptor (MAINCPU `0x6ee98 + stage*0x100`)

`stage_disp` reads this (`shlo 8` on the stage byte `0x500064`). Layout seen (stage 4):
- `+0x00` u16 = **base model ID** (stage 4: `0x086d`; the decor cluster sits just
  above it, IDs `0x87d..0x8a6`).
- `+0x24..` 4 "header" models (`base+1..base+4`); `+0x2c`, `+0x30..` a few more
  (stage 4 `0x13f6`, `0xacc..0xad1` -- both appear in the capture).
- `+0xA0..` a longer ID list (stage 4 `0x26f..0x488`) -- shared/LOD models, NOT the
  main decor (resolving it gave 0 overlap with the capture).
- `+0x18`/`+0x1c` small ints (likely counts).

So the main decor is a **base + offset cluster** from `entry+0`, not a flat list; the
capture's gaps within that cluster are the frustum-culled objects a from-ROM export
would recover.

## Status / next

Model table: done and validated -- the hard part. A reliable `geo-stage --from-rom`
still needs the descriptor's **enumeration** pinned exactly (the loop that walks the
descriptor and calls `0x7c60` -- how `base`, the cluster offset and the count combine,
and how `+0x24/+0x30/+0xA0` factor in). The flat-list reading is wrong (0 overlap), so
that loop must be traced before building the mode, rather than guessed. A validatable
interim is the hybrid: from a capture, map its records -> IDs via this table, take the
ID range, and emit every valid ID in it with the capture's camera matrix -- recovering
the culled objects (stage 4: ~24 captured in `0x87d..0x8a6` vs ~42 in the full range).

ROM reconstructed from the manifest `VF2_LOAD32_WORD` interleave (maincpu epr-1838x;
main_data mpr-17560/17561). No ROM bytes committed.
