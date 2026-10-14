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

vf2_status vf2_tile_render_row(
    const uint8_t *main_data,
    size_t main_data_size,
    const uint16_t *entries,
    size_t cols,
    uint8_t *out
)
{
    const size_t stride = cols * VF2_TILE_GLYPH_W;
    size_t col = 0u;

    if (main_data == NULL || entries == NULL || out == NULL || cols == 0u) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    for (col = 0u; col < cols; ++col) {
        uint8_t glyph[VF2_TILE_GLYPH_PIXELS];
        size_t y = 0u;
        const vf2_status status = vf2_tile_font_glyph(
            main_data, main_data_size, vf2_tile_name_char(entries[col]), glyph
        );
        if (status != VF2_OK) {
            return status;
        }
        for (y = 0u; y < VF2_TILE_GLYPH_H; ++y) {
            size_t x = 0u;
            for (x = 0u; x < VF2_TILE_GLYPH_W; ++x) {
                out[y * stride + col * VF2_TILE_GLYPH_W + x] =
                    glyph[y * VF2_TILE_GLYPH_W + x];
            }
        }
    }
    return VF2_OK;
}

uint32_t vf2_tile_palette_rgb555(uint16_t entry)
{
    const uint32_t r5 = (uint32_t)((entry >> 10) & 0x1fu);
    const uint32_t g5 = (uint32_t)((entry >> 5) & 0x1fu);
    const uint32_t b5 = (uint32_t)(entry & 0x1fu);
    const uint32_t r8 = (r5 << 3) | (r5 >> 2);
    const uint32_t g8 = (g5 << 3) | (g5 >> 2);
    const uint32_t b8 = (b5 << 3) | (b5 >> 2);
    return (r8 << 16) | (g8 << 8) | b8;
}
