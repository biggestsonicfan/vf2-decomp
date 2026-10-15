#include "vf2/texture_sheet.h"

/* See include/vf2/texture_sheet.h for the format and its reference-free pin.
 * Fail-closed on bad arguments; no allocation, no I/O. */

vf2_status vf2_texture_sheet_decode(
    const uint8_t *bank,
    size_t bank_size,
    uint32_t width,
    uint32_t height,
    uint8_t *out_index,
    size_t out_size,
    size_t *out_count
)
{
    size_t texel_count;
    size_t required_bank_bytes;
    uint32_t row;

    if (out_count != NULL) {
        *out_count = 0u;
    }
    if (bank == NULL || out_index == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (width == 0u || (width % 8u) != 0u || height == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    texel_count = (size_t)width * (size_t)height;
    /* Four bits per texel: two texels per byte. width*height is even because
     * width is a multiple of 8. */
    required_bank_bytes = texel_count / 2u;
    if (bank_size < required_bank_bytes || out_size < texel_count) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }

    for (row = 0u; row < height; ++row) {
        uint32_t words_per_row = width / 8u;
        uint32_t word_in_row;
        size_t row_base = (size_t)row * (size_t)width;
        size_t byte_pos = (size_t)row * (size_t)words_per_row * 4u;

        for (word_in_row = 0u; word_in_row < words_per_row; ++word_in_row) {
            uint32_t word =
                (uint32_t)bank[byte_pos] |
                ((uint32_t)bank[byte_pos + 1u] << 8) |
                ((uint32_t)bank[byte_pos + 2u] << 16) |
                ((uint32_t)bank[byte_pos + 3u] << 24);
            uint32_t low = word & 0xffffu;
            uint32_t high = (word >> 16) & 0xffffu;
            size_t col = row_base + (size_t)word_in_row * 8u;

            byte_pos += 4u;

            out_index[col + 0u] = (uint8_t)((low >> 12) & 0xfu);
            out_index[col + 1u] = (uint8_t)((low >> 8) & 0xfu);
            out_index[col + 2u] = (uint8_t)((low >> 4) & 0xfu);
            out_index[col + 3u] = (uint8_t)(low & 0xfu);
            out_index[col + 4u] = (uint8_t)((high >> 12) & 0xfu);
            out_index[col + 5u] = (uint8_t)((high >> 8) & 0xfu);
            out_index[col + 6u] = (uint8_t)((high >> 4) & 0xfu);
            out_index[col + 7u] = (uint8_t)(high & 0xfu);
        }
    }

    if (out_count != NULL) {
        *out_count = texel_count;
    }
    return VF2_OK;
}

uint8_t vf2_texture_sheet_index_to_gray(uint8_t index)
{
    if (index > 15u) {
        return 0u;
    }
    return (uint8_t)(index * 17u);
}
