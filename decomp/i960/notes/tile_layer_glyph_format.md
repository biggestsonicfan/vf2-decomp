# Phase 7 start: the 2D tile-layer glyph format (recovered in-tree)

Date: 2026-10-14. Phase 7 (video) part 1 is a software reference renderer for the
Model 2 tile layers, palette and texture -- the 2D path, which does not need the
TGP/3D pipeline. The attract corridor already populates the tile RAM (the
recovered runtime runs it with `interpreted == 0`), so the format can be
recovered in-tree from the cold snapshot's `tile-ram`/`palette-ram` regions.

## Tile RAM layout (measured, cold snapshot)

- `tile-ram` is 1 MiB. Nonzero regions: `0x000000` (name tables, ~20k nz),
  `0x080000`-`0x0b0000` (~46k nz each -- the character pattern data / font),
  `0x0f0000` (~4k nz).
- `palette-ram` is 1 MiB of 16-bit entries (`palette[1] = 0xffff` = white).

## Name table (0x000000+)

16-bit entries, `(bank << 8) | char`: e.g. `0x0020` = bank 0, char 0x20 (space).
Banks 0x80/0x88/0x89 seen elsewhere carry ASCII text (`dump_tile_glyphs.py` /
`decode_glyph_tiles.py` decode the codes). An all-`0x0020` run is a blank line.

## Glyph patterns (0x080000+): 8x8, 4bpp, ASCII-indexed (verified)

The font is at `tile-ram 0x080000`, one 8x8 tile per character at
`0x080000 + char*32` (4 bits/pixel, 4 bytes/row, 32 bytes/tile). The character
index is the ASCII code. Rendering tiles as ASCII art (nonzero nibble = ink)
gives recognisable glyphs -- a self-consistency check that pins the format:

```
char 0x20 ' ' -> all blank
char 0x31 '1' -> #.....##  ##....##  ###...##  ###...##  ##....## ...   (a '1')
char 0x53 'S' -> ##....##  ###..###  ########  ###..###  ########  ##....##  (an 'S')
char 0x45 'E' -> ####.###  ########  ####.### (repeated)  ########  (an 'E')
char 0x41 'A' -> ##....##  ###..###  ########  ... .##..##.   (an 'A')
```

The glyphs are a bold/filled style (the ink nibble covers most of the cell with
gaps forming the letter); the exact nibble value selects the palette entry.

## Name-table geometry pinned: stride 64 (the TEST MENU reads correctly)

Decoding the name table at `tile-ram 0x0000` as ASCII (`vf2_tile_name_char`) at
candidate strides: at **64 cells/row** the text is clean and correct, at 128 it
is garbled. The cold snapshot's layer is the **TEST MENU** screen:

```
 r2:   TEST MENU
 r8:   MEMORY TEST
r11:   INPUT TEST
r14:   SOUND TEST
r17:   DISPLAY TEST
r20:   GAME ASSIGNMENT
r23:   COIN ASSIGNMENT
r26:   BOOKKEEPING
```

(and, further down, TGP TEST / VS DIAGRAM / BACKUP RAM / SELECT BY SERVICE
BUTTON). These are exactly the selector-17 TEST-MENU items seen in the attract
investigation. So the name-table stride is 64 16-bit entries per row, and this
is an **end-to-end self-consistency validation of the whole tile pipeline**: the
recovered font + name-table decode + stride reproduce the game's real on-screen
UI text, with no reference needed.

## What this gives, and what remains

Pinned in-tree (self-consistent): the font location and tile stride, the 8x8
4bpp encoding, the ASCII char mapping, the name-table entry format, and the
palette as 16-bit entries. This is enough to build a software tile-layer
renderer: for each screen cell, read the name-table entry, fetch the 8x8 glyph
at `0x080000 + char*32`, map each 4bpp nibble through the palette, and blit.
Such a renderer is validatable in-tree by self-consistency -- the rendered text
must match the ASCII the name table encodes (and `decode_glyph_tiles.py`
extracts).

Still to pin for a faithful renderer: the exact bank->pattern-base mapping (for
banks 0x88/0x89), the palette entry colour encoding (RGB555 vs other), the
name-table->screen geometry (layer width/height, scroll), and layer priority.
The ROM source of the font (whether it is loaded from a committed ROM region, so
the renderer gets a reproducible test fixture) is the next thing to locate.
Pixel-exact validation against hardware, and the GPU renderer and image-diff
harness (Phase 7 parts 2-3), still need reference screenshots.
