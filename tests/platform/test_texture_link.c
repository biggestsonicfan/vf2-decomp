/* Polygon->texture link (geometrizer decode) tests. Synthetic data only -- no
 * ROM, no capture, so this runs in CI. Validates the texture-header bitfields,
 * the signed per-primitive header-pointer advance, the 13.3 UV conversion, and
 * the 2x2-swizzled / folded sheet texel addressing. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/texture_link.h"

static int failures = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #expr);  \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

static void put_u32(uint8_t *p, size_t byte, uint32_t v)
{
    p[byte + 0u] = (uint8_t)(v & 0xffu);
    p[byte + 1u] = (uint8_t)((v >> 8) & 0xffu);
    p[byte + 2u] = (uint8_t)((v >> 16) & 0xffu);
    p[byte + 3u] = (uint8_t)((v >> 24) & 0xffu);
}

static void test_header_decode(void)
{
    /* th0=0x63c3: width=32<<3=256, height=32<<0=32; bits6/7 set but bits8/9
     * (mirror) also set, so wrap is disabled; renderer=(>>13)&3=3.
     * th1=0x00ff: luma_base=0xff<<7=0x7f80. th2=0x1a20: tex_x=32*0x20=1024,
     * tex_y=32*8=256, sheet bit 0x1000 set. th3=0x9740: color=(>>6)&0x3ff=605. */
    uint16_t th[4] = { 0x63c3u, 0x00ffu, 0x1a20u, 0x9740u };
    vf2_texture_header h;
    memset(&h, 0xcc, sizeof(h));
    vf2_texture_header_decode(th, &h);
    CHECK(h.tex_width == 256u);
    CHECK(h.tex_height == 32u);
    CHECK(h.mirror_x == 1u && h.mirror_y == 1u);
    CHECK(h.wrap_x == 0u && h.wrap_y == 0u); /* mirror disables wrap */
    CHECK(h.renderer == 3u);
    CHECK(h.luma_base == 0x7f80u);
    CHECK(h.tex_x == 1024u);
    CHECK(h.tex_y == 256u);
    CHECK(h.sheet == 1u);
    CHECK(h.color_base == 605u);

    /* A header with wrap set and mirror clear keeps wrap. th0=0x00c3:
     * width=256, height=32, bit6=1,bit7=1, bit8=0,bit9=0 -> wrap 1,1. */
    {
        uint16_t th2[4] = { 0x00c3u, 0u, 0u, 0u };
        vf2_texture_header h2;
        vf2_texture_header_decode(th2, &h2);
        CHECK(h2.wrap_x == 1u && h2.wrap_y == 1u);
        CHECK(h2.mirror_x == 0u && h2.mirror_y == 0u);
        CHECK(h2.sheet == 0u);
    }
}

static void test_header_advance(void)
{
    CHECK(vf2_texture_header_advance_words(0x49401a01u) == 4);  /* (>>12)&1f = 1 */
    CHECK(vf2_texture_header_advance_words(0x00001000u) == 4);  /* +1 */
    CHECK(vf2_texture_header_advance_words(0x0001f000u) == -4); /* 0x1f -> -1 */
    CHECK(vf2_texture_header_advance_words(0x00010000u) == -64);/* 0x10 -> -16 */
    CHECK(vf2_texture_header_advance_words(0x00000000u) == 0);
}

static void test_uv(void)
{
    CHECK(vf2_texture_uv_texels(1024u) == 128.0f);
    CHECK(vf2_texture_uv_texels(4u) == 0.5f);
    CHECK(vf2_texture_uv_texels(0u) == 0.0f);
}

static void test_sheet_texel(void)
{
    /* word0 at byte 0 = 0x56781234: low16=0x1234 is the 2x2 block
     * {(0,0)=1,(1,0)=2,(0,1)=3,(1,1)=4}; high16=0x5678 is the next block
     * {(2,0)=5,(3,0)=6,(2,1)=7,(3,1)=8}. word1 low16=0xABCD -> (4,0)=0xA. */
    uint8_t bank[8];
    put_u32(bank, 0u, 0x56781234u);
    put_u32(bank, 4u, 0x0000abcdu);

    CHECK(vf2_texture_sheet_texel(bank, sizeof(bank), 0u, 0u) == 0x1u);
    CHECK(vf2_texture_sheet_texel(bank, sizeof(bank), 1u, 0u) == 0x2u);
    CHECK(vf2_texture_sheet_texel(bank, sizeof(bank), 0u, 1u) == 0x3u);
    CHECK(vf2_texture_sheet_texel(bank, sizeof(bank), 1u, 1u) == 0x4u);
    CHECK(vf2_texture_sheet_texel(bank, sizeof(bank), 2u, 0u) == 0x5u);
    CHECK(vf2_texture_sheet_texel(bank, sizeof(bank), 3u, 0u) == 0x6u);
    CHECK(vf2_texture_sheet_texel(bank, sizeof(bank), 2u, 1u) == 0x7u);
    CHECK(vf2_texture_sheet_texel(bank, sizeof(bank), 3u, 1u) == 0x8u);
    CHECK(vf2_texture_sheet_texel(bank, sizeof(bank), 4u, 0u) == 0xau);
    /* out of range -> 0, no crash */
    CHECK(vf2_texture_sheet_texel(bank, sizeof(bank), 2000u, 1000u) == 0u);
    CHECK(vf2_texture_sheet_texel(NULL, 0u, 0u, 0u) == 0u);
}

static void test_sheet_fold(void)
{
    /* x>=1024 folds to physical: x-=1024, y^=1024. (1024,0) -> (0,1024):
     * offset=(1024/2)*512 + 0 = 262144, u32 index 131072, byte 524288.
     * Put low16=0xf000 there -> (y even ->>8, x even ->>4) -> 0xf. */
    size_t size = 0x90000u;
    uint8_t *bank = (uint8_t *)calloc(size, 1u);
    if (bank == NULL) {
        fprintf(stderr, "test_sheet_fold: alloc failed, skipping\n");
        return;
    }
    put_u32(bank, 524288u, 0x0000f000u);
    CHECK(vf2_texture_sheet_texel(bank, size, 1024u, 0u) == 0xfu);
    free(bank);
}

int main(void)
{
    test_header_decode();
    test_header_advance();
    test_uv();
    test_sheet_texel();
    test_sheet_fold();
    if (failures != 0) {
        fprintf(stderr, "texture-link tests FAILED: %d\n", failures);
        return 1;
    }
    printf("texture-link tests passed\n");
    return 0;
}
