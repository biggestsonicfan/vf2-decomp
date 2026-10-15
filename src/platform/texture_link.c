#include "vf2/texture_link.h"

/* See include/vf2/texture_link.h and
 * decomp/i960/notes/texture_polygon_link_v0831.md for the recovered format and
 * its validation. Pure decode: no allocation, no I/O, fail-closed. */

void vf2_texture_header_decode(const uint16_t th[4], vf2_texture_header *out)
{
    uint32_t th0;
    uint32_t th1;
    uint32_t th2;
    uint32_t th3;

    if (th == NULL || out == NULL) {
        return;
    }
    th0 = th[0];
    th1 = th[1];
    th2 = th[2];
    th3 = th[3];

    out->tex_width = 32u << (th0 & 7u);
    out->tex_height = 32u << ((th0 >> 3) & 7u);
    out->mirror_x = (uint8_t)((th0 >> 8) & 1u);
    out->mirror_y = (uint8_t)((th0 >> 9) & 1u);
    /* smooth wrapping is disabled on an axis that mirrors */
    out->wrap_x = (uint8_t)(out->mirror_x ? 0u : ((th0 >> 6) & 1u));
    out->wrap_y = (uint8_t)(out->mirror_y ? 0u : ((th0 >> 7) & 1u));
    out->renderer = (uint8_t)((th0 >> 13) & 3u);

    out->luma_base = (th1 & 0xffu) << 7;

    out->tex_x = 32u * (th2 & 0x3fu);
    out->tex_y = 32u * ((th2 >> 6) & 0x1fu);
    out->sheet = (th2 & 0x1000u) ? 1u : 0u;

    out->color_base = (uint16_t)((th3 >> 6) & 0x3ffu);
}

int32_t vf2_texture_header_advance_words(uint32_t attr)
{
    int32_t tho = (int32_t)((attr >> 12) & 0x1fu);

    if (tho & 0x10) {
        tho -= 0x20; /* sign-extend the 5-bit field */
    }
    return tho * 4;
}

float vf2_texture_uv_texels(uint16_t raw)
{
    return (float)raw / 8.0f;
}

uint8_t vf2_texture_sheet_texel(
    const uint8_t *bank,
    size_t bank_size,
    uint32_t x,
    uint32_t y
)
{
    uint32_t offset;
    size_t byte;
    uint32_t word;
    uint32_t texel;

    if (bank == NULL) {
        return 0u;
    }
    /* the logical 2048x1024 sheet is stored folded as physical 1024x2048 */
    if (x >= 1024u) {
        x -= 1024u;
        y ^= 1024u;
    }
    offset = (y >> 1) * 512u + (x >> 1);
    byte = (size_t)(offset >> 1) * 4u;
    if (byte + 4u > bank_size) {
        return 0u;
    }
    word = (uint32_t)bank[byte] |
           ((uint32_t)bank[byte + 1u] << 8) |
           ((uint32_t)bank[byte + 2u] << 16) |
           ((uint32_t)bank[byte + 3u] << 24);
    texel = word;
    if (offset & 1u) {
        texel >>= 16;
    }
    if ((y & 1u) == 0u) {
        texel >>= 8;
    }
    if ((x & 1u) == 0u) {
        texel >>= 4;
    }
    return (uint8_t)(texel & 0xfu);
}
