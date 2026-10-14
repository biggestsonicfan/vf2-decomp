#ifndef VF2_TILE_H
#define VF2_TILE_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/status.h"

/* Phase 7 (video) part 1: the 2D tile layer. Recovered in-tree from the attract
 * tile RAM, which is a verbatim copy of the font table in the main_data ROM
 * (decomp/i960/notes/tile_layer_glyph_format.md). The font is an ASCII-indexed
 * table of 8x8 4bpp glyphs: character c is at
 * VF2_TILE_FONT_BASE + c * VF2_TILE_GLYPH_BYTES in main_data, four bytes per
 * row, the low nibble the even pixel and the high nibble the odd pixel. */
enum {
    VF2_TILE_FONT_BASE = 0x00805cb8u,
    VF2_TILE_GLYPH_BYTES = 32u,
    VF2_TILE_GLYPH_W = 8u,
    VF2_TILE_GLYPH_H = 8u,
    VF2_TILE_GLYPH_PIXELS = 64u
};

/* Extract the 8x8 glyph for an ASCII character from the main_data ROM into
 * out[64], one 4-bit palette index per pixel, row-major (y*8 + x). Returns
 * VF2_OK, or VF2_ERROR_OUT_OF_BOUNDS if main_data is too small / arguments are
 * null. A zero glyph (e.g. space, 0x20) yields all-zero indices. */
vf2_status vf2_tile_font_glyph(
    const uint8_t *main_data,
    size_t main_data_size,
    uint8_t ascii,
    uint8_t out[VF2_TILE_GLYPH_PIXELS]
);

/* Name-table entry format: 16-bit little-endian (bank << 8) | char. */
uint8_t vf2_tile_name_char(uint16_t entry);
uint8_t vf2_tile_name_bank(uint16_t entry);

/* Render a row of `cols` name-table entries into out, a (cols*8) x 8 buffer of
 * 4-bit glyph indices, row-major (row-major over the whole strip: index
 * out[y*(cols*8) + col*8 + x]). Each cell blits the 8x8 font glyph for the
 * entry's character. Returns VF2_OK, or VF2_ERROR_OUT_OF_BOUNDS on null/short
 * arguments. This is the tile-layer pixel-index plane, before palette lookup;
 * it is validatable in-tree by self-consistency (each cell equals the font
 * glyph for its character). */
vf2_status vf2_tile_render_row(
    const uint8_t *main_data,
    size_t main_data_size,
    const uint16_t *entries,
    size_t cols,
    uint8_t *out
);

/* Decode a 16-bit Model 2 palette entry to 0x00RRGGBB. The entries are RGB555
 * with bit 15 a flag: bits [14:10] red, [9:5] green, [4:0] blue, each 5-bit
 * channel expanded to 8 bits. Validated self-consistently against the attract
 * palette (0xffff -> white, 0xfc00 -> red, 0x83e0 -> green, 0x801f -> blue;
 * decomp/i960/notes/tile_layer_glyph_format.md). */
uint32_t vf2_tile_palette_rgb555(uint16_t entry);

#endif
