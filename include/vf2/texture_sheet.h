#ifndef VF2_TEXTURE_SHEET_H
#define VF2_TEXTURE_SHEET_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/status.h"

/* Texture-sheet decoder (3D-model extraction, textures slice).
 *
 * A Model 2 3D texture bank (texture RAM 0x12000000 / 0x12400000, 2 MiB each)
 * holds one square sheet of 4-bit luma-index texels. The packing, confirmed
 * bit-exact against a MAME fight capture (0 mismatches over 113,360 sampled
 * texels vs the reference sheet), is:
 *
 *   - each little-endian 32-bit word = two 16-bit halves, LOW half first;
 *   - each 16-bit half = four 4-bit texels, MOST-significant nibble first;
 *   - so a word yields 8 texels in order lo[0..3] then hi[0..3];
 *   - the sheet is `width` texels wide (a multiple of 8) by `height` tall;
 *     a 2048x2048 sheet is exactly one 2 MiB bank (2048*2048*4bit = 2 MiB).
 *
 * The 4-bit value is a LUMA/intensity index; the grayscale sheet (index*17 to
 * 8 bits) is the image a human recognizes (face, eyes, hair, cloth). Full
 * colour additionally needs the luma RAM (0x12800000) and the palette, and the
 * per-polygon sheet rectangle + UVs (the polygon->texture link, which is a
 * video-board geometrizer concern) -- none of that is applied here. See
 * decomp/i960/notes/texture_rom_format_v0827.md and
 * tgp_microcode_no_texture_decode_v0830.md. */

/* Decode a texture bank into one 4-bit luma index per texel.
 *
 * bank/bank_size: the raw texture-RAM bank bytes. width/height: sheet size in
 * texels; width must be a positive multiple of 8 and height positive.
 * out_index/out_size: caller buffer receiving one byte per texel, value 0..15,
 * row-major (out_index[row*width + col]); out_size must be >= width*height.
 * *out_count (optional, may be NULL) receives width*height.
 *
 * Returns VF2_OK; VF2_ERROR_INVALID_ARGUMENT on a NULL bank/out, a width that
 * is not a positive multiple of 8, or a zero height; VF2_ERROR_OUT_OF_BOUNDS if
 * the bank is too small to hold width*height 4-bit texels or out is too small. */
vf2_status vf2_texture_sheet_decode(
    const uint8_t *bank,
    size_t bank_size,
    uint32_t width,
    uint32_t height,
    uint8_t *out_index,
    size_t out_size,
    size_t *out_count
);

/* Convenience: scale a 4-bit index (0..15) to an 8-bit grayscale level
 * (index * 17, i.e. 0x0->0x00 .. 0xf->0xff), the value the reference sheet
 * stores. Returns 0 for indices > 15. */
uint8_t vf2_texture_sheet_index_to_gray(uint8_t index);

#endif
