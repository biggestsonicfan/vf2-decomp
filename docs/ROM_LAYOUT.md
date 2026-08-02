# ROM layout

The supported set is:

- **Game:** Virtua Fighter 2 Version 2.1
- **Board:** Sega Model 2A
- **MAME set name:** `vf2`
- **Game board:** 833-11341
- **ROM board:** 834-11342

The exact file-level hashes are stored in
`config/vf2_v22_roms.csv`.

## Reconstructed regions

| Region | Output | Size | Purpose |
|---|---|---:|---|
| `maincpu` | `maincpu.bin` | `0x200000` | i960 address region |
| `main_data` | `main_data.bin` | `0x2400000` | game data |
| `copro_data` | `copro_data.bin` | `0x800000` | empty/zero-filled for this set |
| `polygons` | `polygons.bin` | `0x2000000` | model and polygon data |
| `textures` | `textures.bin` | `0x1000000` | texture data, erased space filled with `0xff` |
| `audiocpu` | `audiocpu.bin` | `0x080000` | byte-swapped 68000 program |
| `samples` | `samples.bin` | `0x800000` | byte-swapped SCSP samples |
| `copro_tgp_tables` | `copro_tgp_tables.bin` | `0x040000` | TGP lookup tables |
| `other_data` | `other_data.bin` | `0x080000` | reciprocal and inverse-square-root tables |
| `video_unk` | `video_unk.bin` | `0x200000` | Model 2A video-board lookup data |

`vf2rom extract` reconstructs these layouts but never modifies the source ROMs.
Generated `.bin` files are ignored by Git.
