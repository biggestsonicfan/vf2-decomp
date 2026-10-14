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
    VF2_TILE_GLYPH_PIXELS = 64u,
    /* Name-table row stride in 16-bit entries, pinned by the TEST MENU reading
     * correctly at this width (decomp/i960/notes/tile_layer_glyph_format.md). */
    VF2_TILE_NAME_STRIDE = 64u
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

/* Render a row of `cols` name-table entries to RGB: out is a (cols*8) x 8
 * buffer of 0x00RRGGBB, out[y*(cols*8) + col*8 + x]. Each pixel's 4-bit glyph
 * index selects palette entry (palette_base + index) from palette_ram (16-bit
 * RGB555 entries, little-endian), decoded via vf2_tile_palette_rgb555. This
 * composes the validated font / row / palette pieces; palette_base (the per-tile
 * palette bank) is a parameter rather than assumed, pending its name-table
 * attribute mapping. Returns VF2_ERROR_OUT_OF_BOUNDS on null/short arguments. */
vf2_status vf2_tile_render_row_rgb(
    const uint8_t *main_data,
    size_t main_data_size,
    const uint16_t *entries,
    size_t cols,
    const uint8_t *palette_ram,
    size_t palette_size,
    size_t palette_base,
    uint32_t *out
);

/* Decode one name-table cell (row, col) from a raw tile-RAM image to its ASCII
 * character, using VF2_TILE_NAME_STRIDE. Returns 0 on out-of-range. This is the
 * text self-consistency hook: the TEST MENU reads correctly through it. */
uint8_t vf2_tile_name_char_at(
    const uint8_t *tile_ram, size_t tile_ram_size, size_t row, size_t col
);

/* Render a `cols` x `rows` block of the tile layer (from name-table cell (0,0),
 * stride VF2_TILE_NAME_STRIDE, in the raw tile-RAM image) to out, a (cols*8) x
 * (rows*8) RGB buffer, out[py*(cols*8) + px]. Composes the per-row renderer with
 * the font and palette. Returns VF2_ERROR_OUT_OF_BOUNDS on null/short args. */
vf2_status vf2_tile_render_layer(
    const uint8_t *tile_ram,
    size_t tile_ram_size,
    size_t cols,
    size_t rows,
    const uint8_t *main_data,
    size_t main_data_size,
    const uint8_t *palette_ram,
    size_t palette_size,
    size_t palette_base,
    uint32_t *out
);

#endif
