#include "vf2/tile.h"

/* See include/vf2/tile.h and decomp/i960/notes/tile_layer_glyph_format.md. */

vf2_status vf2_tile_font_glyph(
    const uint8_t *main_data,
    size_t main_data_size,
    uint8_t ascii,
    uint8_t out[VF2_TILE_GLYPH_PIXELS]
)
{
    const size_t offset =
        (size_t)VF2_TILE_FONT_BASE + (size_t)ascii * VF2_TILE_GLYPH_BYTES;
    size_t y = 0u;
    size_t x = 0u;

    if (main_data == NULL || out == NULL ||
        offset + VF2_TILE_GLYPH_BYTES > main_data_size) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    for (y = 0u; y < VF2_TILE_GLYPH_H; ++y) {
        for (x = 0u; x < VF2_TILE_GLYPH_W; ++x) {
            const uint8_t byte = main_data[offset + y * 4u + (x >> 1)];
            out[y * VF2_TILE_GLYPH_W + x] =
                (x & 1u) ? (uint8_t)(byte >> 4u) : (uint8_t)(byte & 0x0fu);
        }
    }
    return VF2_OK;
}

uint8_t vf2_tile_name_char(uint16_t entry)
{
    return (uint8_t)(entry & 0x00ffu);
}

uint8_t vf2_tile_name_bank(uint16_t entry)
{
    return (uint8_t)(entry >> 8u);
}
